# DriverHub Testing Plan

## Overview

This document describes the testing strategy for DriverHub's component runner
architecture. Tests are organized in three tiers: unit tests for individual
components, integration tests using the Fuchsia driver testing framework, and
end-to-end tests with real `.ko` modules packaged as Fuchsia components.

## Test Tiers

### Tier 1: Unit Tests

Pure logic tests that don't require process creation or FIDL. These run as
standard gtest binaries.

#### Existing (unchanged)

| Test file | Tests | Status |
|-----------|-------|--------|
| `tests/dependency_sort_test.cc` | `TopologicalSortModules` | No changes needed |
| `tests/modinfo_parser_test.cc` | `.modinfo` section parsing, ELF extraction | No changes needed |

#### New

| Test file | Component under test | Key scenarios |
|-----------|---------------------|---------------|
| `tests/ko_runner_test.cc` | `KoRunner` | Start() parameter validation, colocation group lookup, ComponentController lifecycle, metadata extraction from ComponentStartInfo |
| `tests/symbol_registry_fidl_test.cc` | `SymbolRegistryServer` | Register single/batch symbols, Resolve by name, ResolveBatch, duplicate registration (overwrite), resolve unknown symbol (error), cleanup on module removal |
| `tests/symbol_proxy_test.cc` | `SymbolProxy` | Argument serialization for various C types (int, pointer, struct), return value deserialization, VMO data symbol mapping (read-only and read-write), error propagation |
| `tests/process_manager_test.cc` | `ProcessManager` | Process creation, VMAR allocation, crash signal detection (ZX_PROCESS_TERMINATED), process cleanup |

### Tier 2: Integration Tests

Tests that exercise cross-component interactions. Use the Fuchsia driver
testing framework (`driver_testing_cpp`) where applicable.

#### Migrated from BusDriver

| Test file | Migration notes |
|-----------|----------------|
| `tests/rtc_ops_test.cc` | Replace `BusDriver` with `KoRunner::Start()`. Package `rtc-test.ko` as a component with `runner: "ko"`. Verify RTC ops still work through the shared library. |
| `tests/dt_rtc_test.cc` | DTB parsing tests unchanged. Module loading tests migrate to use runner. FdtBuilder class reused as-is. |

#### New

| Test file | Scenario |
|-----------|----------|
| `tests/runner_integration_test.cc` | Launch `test_module.ko` via `KoRunner::Start()`. Verify `module_init()` runs (check log output). Call `Stop()`, verify `module_exit()` runs. Verify process is cleaned up. |
| `tests/colocation_test.cc` | Load `export_module.ko` and `import_module.ko` in the same colocation group. Verify `exported_add` resolves to a direct in-process pointer (not a proxy). Verify `import_module` init succeeds and calls `exported_add(3,4) == 7`. |
| `tests/cross_process_symbol_test.cc` | Load `export_module.ko` in Process A, `import_module.ko` in Process B (no colocation). Verify `exported_add` resolves via `SymbolProxy`. Verify the FIDL round-trip returns correct results. |
| `tests/crash_isolation_test.cc` | Load `crash_module.ko` in an isolated process. Verify the process crashes. Verify the runner process survives. Verify sibling modules in other processes are unaffected. Load `crash_module.ko` in a colocation group with another module — verify the entire group goes down but the runner survives. |

### Tier 3: End-to-End Tests on Fuchsia

Full component lifecycle tests using `fx test`. Each test `.ko` is packaged as
a Fuchsia component with a `.cml` manifest declaring `runner: "ko"`.

#### Test component manifests

```json5
// meta/test-modules/test_module.cml
{
    program: {
        runner: "ko",
        binary: "data/modules/test_module.ko",
    },
    use: [
        { protocol: "fuchsia.driverhub.SymbolRegistry" },
    ],
}
```

```json5
// meta/test-modules/colocation-wifi.cml
{
    program: {
        runner: "ko",
        binary: "data/modules/wifi_driver_stub.ko",
        colocation_group: "wifi-test",
        group_modules: [
            "data/modules/cfg80211_stub.ko",
            "data/modules/wifi_driver_stub.ko",
        ],
    },
    use: [
        { protocol: "fuchsia.driverhub.SymbolRegistry" },
    ],
}
```

#### E2E test scenarios

| Scenario | What it validates |
|----------|------------------|
| Single isolated module | Runner creates process, loads .ko, module_init runs, module_exit on stop |
| Colocated WiFi stack | cfg80211_stub + wifi_driver_stub share process, hundreds of EXPORT_SYMBOL resolved directly |
| Cross-process EXPORT_SYMBOL | export_module in Process A, import_module in Process B, function proxy works |
| Crash isolation | crash_module dies, runner and siblings survive, SymbolRegistry cleaned up |
| Module restart | Stop and re-Start a module, verify clean lifecycle |
| DT-driven loading | Runner reads DTB, discovers modules, launches in correct dependency order |

## New Test Kernel Modules

### `tests/crash_module.c`

Deliberately crashes during `module_init()` to test process isolation.

```c
// Triggers a null pointer dereference during init.
// The runner should detect the process crash and clean up.
static int __init crash_init(void) {
    printk(KERN_INFO "crash_module: about to crash\n");
    int *p = NULL;
    *p = 42;  // SIGSEGV
    return 0;
}
```

### `tests/data_export_module.c`

Exports both function and data symbols to test both proxy paths.

```c
int shared_value = 42;
EXPORT_SYMBOL(shared_value);         // data symbol → VMO mapping

int exported_compute(int x) {
    return x * shared_value;
}
EXPORT_SYMBOL(exported_compute);     // function symbol → SymbolProxy
```

### `tests/data_import_module.c`

Imports both function and data symbols from `data_export_module`.

```c
extern int shared_value;             // resolved via VMO mapping
extern int exported_compute(int x);  // resolved via SymbolProxy

static int __init data_import_init(void) {
    printk("shared_value = %d\n", shared_value);           // 42
    printk("exported_compute(3) = %d\n", exported_compute(3)); // 126
    return 0;
}
```

### `tests/wifi_stack_test/cfg80211_stub.c`

Minimal cfg80211-like module that exports a set of symbols to simulate a
tightly-coupled WiFi subsystem (for colocation testing).

```c
// Exports ~20 symbols mimicking cfg80211's interface:
// cfg80211_scan_done, cfg80211_connect_result, wiphy_new, etc.
// Each is a trivial stub that logs and returns success.
```

### `tests/wifi_stack_test/wifi_driver_stub.c`

Imports all cfg80211_stub symbols. When colocated, all resolve directly.
When isolated, all go through SymbolProxy (performance comparison).

## Build Integration

### New BUILD.gn targets

```gn
# Test runner integration tests
executable("driverhub-runner-test-bin") {
  testonly = true
  sources = [
    "tests/ko_runner_test.cc",
    "tests/symbol_registry_fidl_test.cc",
    "tests/symbol_proxy_test.cc",
    "tests/process_manager_test.cc",
    "tests/runner_integration_test.cc",
    "tests/colocation_test.cc",
    "tests/cross_process_symbol_test.cc",
    "tests/crash_isolation_test.cc",
  ]
  deps = [
    ":driverhub-core",
    ":driverhub-runner-lib",
    "//sdk/fidl/fuchsia.driverhub:fuchsia.driverhub_cpp",
    "//src/lib/fxl/test:gtest_main",
    "//third_party/googletest:gtest",
  ]
}

# Test .ko modules compiled as ET_REL
action("crash_module_ko") {
  # Cross-compile crash_module.c to .ko
}

action("data_export_module_ko") { ... }
action("data_import_module_ko") { ... }
action("cfg80211_stub_ko") { ... }
action("wifi_driver_stub_ko") { ... }
```

### Makefile targets (host testing)

```makefile
# New test targets
test-crash:         echo | ./driverhub crash_module.ko
test-data-export:   echo | ./driverhub data_export_module.ko data_import_module.ko
test-wifi-stub:     echo | ./driverhub cfg80211_stub.ko wifi_driver_stub.ko
test-runner:        ./tests/driverhub-runner-tests
```

## Existing Test Migration Plan

| Phase | Action |
|-------|--------|
| Phase 1 (runner infra) | Add new unit tests for runner components. Existing BusDriver tests unchanged. |
| Phase 2 (symbol registry) | Add `symbol_registry_fidl_test.cc`. Existing `symbol_registry_test.cc` unchanged. |
| Phase 3 (colocation) | Add `colocation_test.cc`. Reuse `export_module.c` / `import_module.c`. |
| Phase 4 (proxy) | Add `cross_process_symbol_test.cc`, `symbol_proxy_test.cc`. New test modules. |
| Phase 5 (retire bus driver) | Migrate `rtc_ops_test.cc` and `dt_rtc_test.cc` from BusDriver to KoRunner. Remove old BusDriver tests. |

## Shim Testing Philosophy

DriverHub tests shims via **real module execution**, not mocks:

- Shim subsystems export **test harness functions** (e.g.,
  `driverhub_rtc_get_device()`) for programmatic verification.
- Test `.ko` modules exercise shim APIs end-to-end.
- The `libdriverhub_kmi.so` shared library is the same code used in
  production — tests validate the real artifact.
- Per-process shim state (allocator, workqueues, timers) is tested implicitly
  by running modules in separate processes and verifying independent operation.
