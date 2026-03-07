# DriverHub Runner Architecture

## Overview

DriverHub acts as a **Fuchsia component runner** for Linux GKI `.ko` kernel
modules. Rather than loading all modules into a single bus driver process,
DriverHub implements `fuchsia.component.runner.ComponentRunner` and launches
each module (or group of tightly-coupled modules) in its own process. A shared
library (`libdriverhub_kmi.so`) provides the KMI shim layer, and a central
FIDL service manages cross-module `EXPORT_SYMBOL` resolution.

This design replaces the previous bus-driver-with-child-nodes model. The
previous model loaded all `.ko` files into a single address space and used
in-process symbol tables. The runner model provides:

- **Process isolation**: A buggy module crashes only its own process.
- **Capability scoping**: Each module component declares only the FIDL
  protocols it needs in its `.cml` manifest. The component framework enforces
  this — no artificial symbol-level filtering needed.
- **Clean integration**: Modules appear as normal Fuchsia components. The
  runner slots into the existing driver framework alongside `DriverHostRunner`.
- **Shared shim code**: The KMI shim library is loaded once per process via the
  dynamic linker, not statically linked into a monolithic driver.

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│  Driver Manager                                                  │
│                                                                  │
│  DriverRunner (orchestration, bind matching, topology)           │
│    ├── DriverHostRunner  →  ELF .so drivers (existing)           │
│    └── KoRunner          →  .ko modules (DriverHub)              │
│                                                                  │
└──────────┬───────────────────────────────────────────────────────┘
           │ fuchsia.component.runner.ComponentRunner
           │
┌──────────▼───────────────────────────────────────────────────────┐
│  DriverHub Runner Process                                        │
│                                                                  │
│  ┌────────────────────┐    ┌──────────────────────────────┐      │
│  │ ComponentRunner     │    │ SymbolRegistry               │      │
│  │ FIDL server         │    │ FIDL service                 │      │
│  │                     │    │                              │      │
│  │ • Start()           │    │ • Register(name, module,     │      │
│  │ • creates process   │    │           vmo, offset)       │      │
│  │ • maps shim lib     │    │ • Resolve(name) → address    │      │
│  │ • loads .ko          │    │ • intermodule EXPORT_SYMBOL  │      │
│  │ • calls module_init │    │                              │      │
│  └─────────┬──────────┘    └──────────▲───────────────────┘      │
│            │                          │                          │
│            │ creates                  │ FIDL                     │
└────────────┼──────────────────────────┼──────────────────────────┘
             │                          │
    ┌────────┼──────────────────────────┼─────────────────────┐
    │        │                          │                     │
    │        │    Isolated Processes    │                     │
    │        │    (one per module or    │                     │
    │        │     colocation group)    │                     │
    │        ▼                          │                     │
    │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
    │  │ Process A     │  │ Process B     │  │ Process C     │  │
    │  │               │  │               │  │ (colocated)   │  │
    │  │ libdriverhub  │  │ libdriverhub  │  │ libdriverhub  │  │
    │  │ _kmi.so       │  │ _kmi.so       │  │ _kmi.so       │  │
    │  │               │  │               │  │               │  │
    │  │ usb_wifi.ko   │  │ i2c_touch.ko  │  │ cfg80211.ko   │  │
    │  │               │  │               │  │ iwlwifi.ko    │  │
    │  └──────┬────────┘  └──────┬────────┘  └──────┬────────┘  │
    │         │ FIDL             │ FIDL             │ FIDL      │
    └─────────┼──────────────────┼──────────────────┼───────────┘
              │                  │                  │
       ┌──────▼───────┐  ┌──────▼───────┐  ┌──────▼───────┐
       │ Fuchsia USB   │  │ Fuchsia I2C   │  │ Fuchsia WLAN  │
       │ Stack         │  │ Stack         │  │ Stack         │
       └──────────────┘  └──────────────┘  └──────────────┘
```

## Components

### KoRunner (`src/runner/`)

The core runner component. Implements `fuchsia.component.runner.ComponentRunner`
and manages the lifecycle of `.ko` module processes.

#### Responsibilities

- **Process creation**: For each `Start()` request, creates a new Zircon
  process (or reuses an existing one for colocated modules).
- **Shim library injection**: Provides `libdriverhub_kmi.so` to each module
  process via the loader service / dynamic linker namespace.
- **Module loading**: Uses `elfldltl` to load the `.ko` into the target
  process's VMAR, resolves symbols against the shim library and the central
  symbol registry.
- **Lifecycle management**: Calls `module_init()` on start, `module_exit()` on
  stop. Monitors child processes for crashes and reports to the component
  framework.
- **Device tree integration**: Reads DT configuration to determine module load
  order, dependencies, and colocation groups.

#### Start() flow

```
ComponentRunner::Start(ComponentStartInfo, ServerEnd<ComponentController>)
  │
  ├─ Extract module metadata from ComponentStartInfo
  │   ├─ .ko binary from /pkg/data/modules/
  │   ├─ colocation group from program block
  │   └─ DT node configuration from program block
  │
  ├─ Determine target process
  │   ├─ If colocation group exists and process already running → reuse
  │   └─ Otherwise → create new process via zx::process::create()
  │
  ├─ Set up process namespace
  │   ├─ Map libdriverhub_kmi.so into process (via loader service)
  │   ├─ Provide FIDL channel endpoints for:
  │   │   ├─ SymbolRegistry (for EXPORT_SYMBOL / cross-module resolution)
  │   │   ├─ Hardware protocols declared in component manifest
  │   │   └─ Logging, inspect, etc.
  │   └─ Provide VDSO
  │
  ├─ Load .ko into process VMAR
  │   ├─ Parse ELF ET_REL headers
  │   ├─ Allocate sections (.text, .data, .bss, .rodata)
  │   ├─ Resolve symbols against:
  │   │   ├─ libdriverhub_kmi.so exports (KMI shims)
  │   │   └─ SymbolRegistry (intermodule EXPORT_SYMBOL)
  │   └─ Apply ARM64 relocations
  │
  └─ Start execution
      ├─ Process entry point calls module_init()
      └─ Return ComponentController to framework
```

### Shim Shared Library (`libdriverhub_kmi.so`)

The KMI shim implementations, compiled as a proper shared library. This
replaces the previous static linking model where shims were compiled directly
into the bus driver.

#### Key properties

- **Shared object**: Built as a `.so`, loaded by the dynamic linker into each
  module process. Each process gets its own copy of mutable state.
- **Exact Linux signatures**: All exported functions use `extern "C"` linkage
  and match the Linux KMI function signatures exactly.
- **FIDL-backed**: Subsystem shims (USB, I2C, SPI, GPIO, cfg80211, etc.)
  communicate with Fuchsia services via FIDL channels provided in the process
  namespace.
- **Per-process state**: Each process has its own allocator state, workqueue
  threads, timer lists, etc. No shared mutable state between module processes.

#### Library initialization

When a module process starts, `libdriverhub_kmi.so` initializes itself by:

1. Discovering FIDL channel handles from the process namespace
   (`/svc/fuchsia.driverhub.SymbolRegistry`, hardware protocols, etc.).
2. Setting up the async dispatcher for workqueues, timers, and FIDL calls.
3. Initializing the memory allocator (kmalloc/kfree pool).
4. Registering the `jiffies` timer tick.

#### Contents

The library exports every KMI symbol currently in `SymbolRegistry::RegisterKmiSymbols()`.
This includes:

| Category | Examples |
|----------|----------|
| Memory | `kmalloc`, `kfree`, `vmalloc`, `vfree`, `devm_kmalloc` |
| Logging | `printk`, `dev_err`, `dev_warn`, `dev_info` |
| Sync | `mutex_lock`, `spin_lock`, `wait_event`, `completion` |
| Time | `jiffies`, `msleep`, `ktime_get`, timers |
| IRQ | `request_irq`, `free_irq`, `enable_irq` |
| I/O | `ioremap`, `ioread32`, `iowrite32`, port I/O |
| DMA | `dma_alloc_coherent`, `dma_map_single` |
| Platform | `platform_driver_register`, `platform_get_resource` |
| Workqueue | `schedule_work`, `queue_work`, `flush_workqueue` |
| USB | `usb_register_driver`, `usb_submit_urb` |
| I2C | `i2c_add_driver`, `i2c_transfer`, `i2c_smbus_*` |
| SPI | `spi_register_driver`, `spi_sync`, `spi_async` |
| GPIO | `gpio_request`, `gpiod_get`, `gpio_to_irq` |
| WiFi | `wiphy_new`, `ieee80211_alloc_hw`, `cfg80211_*` |
| Input | `input_allocate_device`, `input_report_abs` |
| DRM/KMS | `drm_dev_alloc`, `drm_connector_init` |
| Block | `blk_mq_*`, `add_disk`, `register_blkdev` |
| SCSI | `scsi_host_alloc`, `scsi_add_host_with_dma` |
| VFS | `cdev_add`, `misc_register`, `proc_create`, `debugfs_*` |
| Clock | `clk_prepare_enable`, `clk_get_rate`, `clk_set_rate` |
| Regulator | `regulator_enable`, `regulator_set_voltage` |
| PM | `pm_runtime_get_sync`, `pm_runtime_put` |
| Thermal | `thermal_zone_device_register`, `hwmon_*` |
| IIO | `iio_device_alloc`, `iio_device_register` |
| Pinctrl | `pinctrl_get`, `pinctrl_select_state` |
| Watchdog | `watchdog_register_device`, `watchdog_init_timeout` |
| NVMEM | `nvmem_register`, `nvmem_cell_read` |
| Power | `power_supply_register`, `power_supply_changed` |
| PWM | `pwm_get`, `pwm_enable`, `pwmchip_add` |
| LED | `led_classdev_register`, `led_set_brightness` |

### Symbol Registry FIDL Service (`fuchsia.driverhub.SymbolRegistry`)

A FIDL protocol served by the runner process. Manages cross-module symbol
registration and resolution for `EXPORT_SYMBOL`.

```fidl
library fuchsia.driverhub;

/// Identifies a symbol exported by a module.
type ExportedSymbol = table {
    /// Symbol name (e.g. "cfg80211_scan_done").
    1: name string:MAX;

    /// VMO containing the module's loaded code/data.
    /// Mapped read-only into importing processes.
    2: vmo zx.Handle:VMO;

    /// Offset within the VMO where the symbol resides.
    3: offset uint64;

    /// Whether this is a function or data symbol.
    4: kind SymbolKind;
};

type SymbolKind = flexible enum : uint8 {
    FUNCTION = 1;
    DATA = 2;
};

/// Result of resolving a symbol.
type ResolvedSymbol = table {
    1: name string:MAX;

    /// For DATA symbols: VMO + offset, mapped into the caller's address space.
    2: vmo zx.Handle:VMO;
    3: offset uint64;

    /// For FUNCTION symbols: client end of a proxy channel.
    /// The caller invokes functions via FIDL messages on this channel.
    4: proxy client_end:SymbolProxy;
};

/// Generic function call proxy for cross-process EXPORT_SYMBOL functions.
protocol SymbolProxy {
    /// Invoke the remote function.
    /// Arguments and return value are serialized as raw bytes.
    /// The runner handles marshalling in/out of the target process.
    Call(struct {
        args vector<byte>:MAX;
    }) -> (struct {
        result vector<byte>:MAX;
    }) error zx.Status;
};

/// Central symbol registry served by the DriverHub runner.
@discoverable
protocol SymbolRegistry {
    /// Register symbols exported by a module.
    /// Called by libdriverhub_kmi.so when a module calls EXPORT_SYMBOL().
    Register(struct {
        module_name string:MAX;
        symbols vector<ExportedSymbol>:MAX;
    }) -> () error zx.Status;

    /// Resolve a symbol by name.
    /// Returns the symbol's location (for data) or a proxy (for functions).
    Resolve(struct {
        name string:MAX;
    }) -> (ResolvedSymbol) error zx.Status;

    /// Resolve multiple symbols in a batch (used during module load).
    ResolveBatch(struct {
        names vector<string:MAX>:MAX;
    }) -> (struct {
        symbols vector<ResolvedSymbol>:MAX;
    }) error zx.Status;
};
```

### Module Colocation

Some Linux kernel modules are tightly coupled and share symbols extensively
via `EXPORT_SYMBOL`. The canonical example is WiFi: `cfg80211.ko` exports
hundreds of symbols consumed by `iwlwifi.ko`, `ath10k.ko`, etc. Proxying
every one of these calls over FIDL would be impractical.

For these cases, DriverHub supports **colocation groups**: multiple `.ko`
modules loaded into the same process, sharing an address space. Within a
colocated group, `EXPORT_SYMBOL` resolution is direct — the symbols resolve
to in-process addresses with no FIDL proxy overhead.

#### Declaring colocation

Colocation is declared in the component manifest's `program` block:

```json5
// meta/iwlwifi.cml
{
    program: {
        runner: "ko",
        binary: "data/modules/iwlwifi.ko",
        colocation_group: "wifi",
    },
}
```

```json5
// meta/cfg80211.cml
{
    program: {
        runner: "ko",
        binary: "data/modules/cfg80211.ko",
        colocation_group: "wifi",
    },
}
```

All modules in the same `colocation_group` are loaded into a single process.
Load order within the group is determined by dependency analysis (`.modinfo`
`depends=` fields).

#### Colocation rules

1. **Same process, same capabilities**: All modules in a colocation group share
   the same process and the same set of FIDL capabilities. The group's `.cml`
   must declare the union of all capabilities needed by any module in the group.
2. **Direct symbol access**: `EXPORT_SYMBOL` within a colocation group resolves
   to direct function pointers. No proxy overhead.
3. **Group lifecycle**: The process starts when the first module in the group is
   needed. All modules in the group are loaded in dependency order. The process
   stops when all modules have been removed.
4. **Fault domain**: A crash in any module in the group takes down the entire
   group. This is an intentional trade-off for performance.

#### When to colocate vs. isolate

| Scenario | Decision | Reason |
|----------|----------|--------|
| WiFi stack (cfg80211 + driver) | Colocate | Hundreds of shared symbols, hot path |
| I2C sensor driver | Isolate | Few/no intermodule deps, benefits from isolation |
| USB driver with shared lib | Colocate if heavy symbol use | Depends on symbol count |
| DRM/KMS stack | Colocate | Complex object model, tight coupling |
| Independent platform driver | Isolate | No intermodule deps |

### Cross-Process Symbol Proxy

For modules that are **not** colocated but still depend on symbols from other
modules, DriverHub provides a generic FIDL-based proxy. This is the
`SymbolProxy` protocol defined above.

#### How it works

```
Module A (Process 1)              DriverHub Runner              Module B (Process 2)
    │                                   │                              │
    │ EXPORT_SYMBOL(foo)                │                              │
    ├──────────────────────────────────►│                              │
    │  Register("foo", vmo, offset,    │                              │
    │           kind=FUNCTION)          │                              │
    │                                   │◄─────────────────────────────┤
    │                                   │  Resolve("foo")              │
    │                                   │                              │
    │                    ┌──────────────┤                              │
    │                    │ Create proxy │                              │
    │                    │ channel pair │                              │
    │                    └──────────────┤                              │
    │                                   │─────────────────────────────►│
    │                                   │  ResolvedSymbol {            │
    │                                   │    proxy: client_end         │
    │                                   │  }                           │
    │                                   │                              │
    │                                   │           foo() called       │
    │  ◄───────────────────────────────────────────────────────────────┤
    │  SymbolProxy.Call(args)           │                              │
    │                                   │                              │
    │  Execute foo(args) locally        │                              │
    │                                   │                              │
    │  ────────────────────────────────────────────────────────────────►
    │  → result                         │                              │
```

#### Function proxy details

The proxy uses a straightforward calling convention:

1. **Caller side** (`libdriverhub_kmi.so` in the importing process): The shim
   library generates a **trampoline function** with the same signature as the
   original. The trampoline serializes arguments into a byte vector, sends a
   `SymbolProxy.Call()` FIDL message, and deserializes the return value.

2. **Callee side** (runner, routing to the exporting process): The runner
   dispatches the `Call()` to a handler thread in the exporting process that
   deserializes the arguments, calls the real function, and returns the result.

3. **Argument serialization**: Arguments are packed using a compact binary
   format matching the function's signature. The runner knows each proxied
   function's signature from its ELF symbol information and generates
   serialization/deserialization code at module load time.

#### Data symbol sharing

For `EXPORT_SYMBOL` of data (global variables), the mechanism is simpler:

1. The exporting module's data section is in a VMO.
2. The VMO is mapped **read-only** into the importing process.
3. The importing process gets a direct pointer to the data.
4. For writable shared data, a **read-write mapping** is used with appropriate
   synchronization (the exporting module is responsible for thread safety, as
   in Linux).

#### Performance considerations

- **Proxy overhead**: Each cross-process function call incurs a FIDL round-trip
  (~1-5 µs on Fuchsia). This is acceptable for control-path calls but not for
  data-path hot loops.
- **Recommendation**: If two modules share more than ~20 function symbols or
  any symbols are called in hot paths, they should be colocated.
- **Batching**: `ResolveBatch()` amortizes the cost of resolving many symbols
  at module load time.

## Module Component Manifest

Each `.ko` module is packaged as a Fuchsia component with a `.cml` manifest.
The manifest declares the runner, binary location, and required capabilities.

### Example: simple I2C sensor driver

```json5
// meta/bmp280.cml — Bosch BMP280 pressure/temperature sensor
{
    include: [
        "syslog/client.shard.cml",
    ],
    program: {
        runner: "ko",
        binary: "data/modules/bmp280.ko",
    },
    use: [
        {
            protocol: "fuchsia.hardware.i2c.Service",
        },
        {
            protocol: "fuchsia.driverhub.SymbolRegistry",
        },
    ],
}
```

### Example: WiFi stack (colocated)

```json5
// meta/iwlwifi-group.cml — Intel WiFi colocated module group
{
    include: [
        "syslog/client.shard.cml",
    ],
    program: {
        runner: "ko",
        binary: "data/modules/iwlwifi.ko",
        colocation_group: "wifi",
        // Modules loaded in dependency order within the group.
        // cfg80211.ko is loaded first because iwlwifi depends on it.
        group_modules: [
            "data/modules/cfg80211.ko",
            "data/modules/iwlwifi.ko",
        ],
    },
    use: [
        {
            protocol: "fuchsia.hardware.wlan.Service",
        },
        {
            protocol: "fuchsia.driverhub.SymbolRegistry",
        },
        {
            protocol: "fuchsia.kernel.VmexResource",
            availability: "optional",
        },
    ],
}
```

## Runner Component Manifest

The DriverHub runner itself is a Fuchsia component:

```json5
// meta/driverhub-runner.cml
{
    include: [
        "inspect/client.shard.cml",
        "syslog/client.shard.cml",
    ],
    program: {
        runner: "elf",
        binary: "bin/driverhub_runner",
    },
    capabilities: [
        {
            runner: "ko",
            path: "/svc/fuchsia.component.runner.ComponentRunner",
        },
        {
            protocol: "fuchsia.driverhub.SymbolRegistry",
        },
    ],
    expose: [
        {
            runner: "ko",
            from: "self",
        },
        {
            protocol: "fuchsia.driverhub.SymbolRegistry",
            from: "self",
        },
    ],
    use: [
        {
            protocol: [
                "fuchsia.kernel.VmexResource",
                "fuchsia.process.Launcher",
            ],
        },
        // The runner needs access to resolve child component packages.
        {
            protocol: "fuchsia.component.resolution.Resolver",
            availability: "optional",
        },
    ],
}
```

## Module Lifecycle

```
Component framework routes Start() to KoRunner
    │
    ├─ Extract .ko binary and metadata from component namespace
    │
    ├─ Determine process target
    │   ├─ colocation_group set? → find or create group process
    │   └─ no group → create new isolated process
    │
    ├─ Set up process
    │   ├─ Map libdriverhub_kmi.so via loader service
    │   ├─ Pass FIDL handles (SymbolRegistry, hardware protocols)
    │   └─ Pass VDSO
    │
    ├─ Load .ko into process VMAR (via elfldltl)
    │   ├─ Resolve KMI symbols → libdriverhub_kmi.so
    │   ├─ Resolve intermodule symbols:
    │   │   ├─ Colocated? → direct address in same process
    │   │   └─ Cross-process? → SymbolProxy trampoline
    │   └─ Apply relocations
    │
    ├─ Call module_init()
    │   └─ Module registers with subsystem shims
    │       └─ Shims open FIDL connections from process namespace
    │
    └─ Return ComponentController
        └─ On Stop: call module_exit(), tear down process

         ─ ─ ─ (runtime) ─ ─ ─

Module crash detected (process exit):
    │
    ├─ Runner receives ZX_PROCESS_TERMINATED signal
    ├─ Clean up SymbolRegistry entries for crashed module
    ├─ If colocated: clean up entire group
    ├─ Report crash via ComponentController.OnEpitaph()
    └─ Component framework handles restart policy
```

## Threading Model

Same as the original design, but now per-process:

| Linux context | DriverHub mapping |
|--------------|------------------|
| Process context | Default async dispatcher thread |
| Workqueues | `async::Loop` worker threads (per-process) |
| Softirq / tasklet | Posted tasks on async dispatcher |
| Hardirq (ISR) | Dedicated interrupt-waiting thread per IRQ |
| Timers | `async::TaskClosure` with deadline |

Each module process has its own async dispatcher and worker threads. There is no
shared threading state between isolated processes.

## Build Integration

```gn
# BUILD.gn additions

# The shared library containing all KMI shims.
shared_library("libdriverhub_kmi") {
  output_name = "libdriverhub_kmi"
  sources = [
    # All shim source files from src/shim/kernel/ and src/shim/subsystem/
  ]
  deps = [
    "//sdk/lib/fdio",
    "//zircon/system/ulib/zx",
    "//zircon/system/ulib/async-loop:async-loop-cpp",
  ]
  # Ensure all KMI symbols are exported.
  configs += [ ":kmi_export_config" ]
}

# The runner binary.
executable("driverhub_runner") {
  sources = [
    "src/runner/main.cc",
    "src/runner/ko_runner.cc",
    "src/runner/symbol_registry_server.cc",
    "src/runner/module_loader.cc",
    "src/runner/process_manager.cc",
  ]
  deps = [
    ":driverhub-core",
    "//sdk/fidl/fuchsia.component.runner:fuchsia.component.runner_cpp",
    "//sdk/fidl/fuchsia.driverhub:fuchsia.driverhub_cpp",
    "//src/lib/elfldltl",
    "//zircon/system/ulib/zx",
  ]
}

# The runner component.
fuchsia_component("driverhub-runner-component") {
  component_name = "driverhub-runner"
  manifest = "meta/driverhub-runner.cml"
  deps = [
    ":driverhub_runner",
    ":libdriverhub_kmi",
  ]
}

# Template for packaging a .ko module as a Fuchsia component.
# Usage:
#   ko_module("bmp280") {
#     ko_binary = "bmp280.ko"
#     manifest = "meta/bmp280.cml"
#   }
template("ko_module") {
  fuchsia_component(target_name) {
    component_name = target_name
    manifest = invoker.manifest
    deps = []
    resources = [
      {
        path = invoker.ko_binary
        dest = "data/modules/${invoker.ko_binary}"
      },
    ]
  }
}
```

## Migration Path

The transition from the current bus-driver model to the runner model:

### Phase 1: Runner infrastructure

- Implement `KoRunner` with `ComponentRunner` FIDL server.
- Build `libdriverhub_kmi.so` from existing shim sources.
- Implement process creation and shim library injection.
- Port the `elfldltl` module loader to work cross-process (load into child
  VMAR instead of self).

### Phase 2: Symbol registry service

- Define `fuchsia.driverhub.SymbolRegistry` FIDL protocol.
- Implement the server in the runner process.
- Add `EXPORT_SYMBOL` support: registration from modules, resolution during
  load.
- Implement data symbol sharing via VMO mapping.

### Phase 3: Colocation groups

- Implement colocation group tracking in the runner.
- Support multiple `.ko` loads into a single process with direct symbol
  resolution.
- Test with WiFi stack (cfg80211 + driver module).

### Phase 4: Function proxy

- Implement `SymbolProxy` FIDL protocol.
- Generate trampolines for cross-process function calls.
- Implement argument serialization based on ELF symbol type information.

### Phase 5: Retire bus driver model

- Remove `DriverHubDriver` (DFv2 bus driver).
- Remove `ModuleChildNode` (in-process child nodes).
- Remove in-process `SymbolRegistry` (replaced by FIDL service).
- Update all component manifests to use `runner: "ko"`.

## Comparison with Previous Design

| Aspect | Bus Driver Model | Runner Model |
|--------|-----------------|--------------|
| Isolation | Single process, all modules | Per-process (or per-group) |
| Crash impact | One module crashes all | Only affected process/group |
| Capability scoping | Artificial (symbol filtering) | Natural (component manifests) |
| Shim library | Statically linked into bus driver | Shared library per process |
| Intermodule symbols | In-process pointer sharing | FIDL proxy or colocation |
| Integration | Custom DFv2 child nodes | Standard Fuchsia components |
| Restart | Must restart entire bus driver | Restart individual modules |
| Memory overhead | Lower (shared address space) | Higher (per-process overhead) |
| Complexity | Simpler loading | More infrastructure required |
