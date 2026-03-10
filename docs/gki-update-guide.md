# Updating DriverHub for New GKI Releases

When Google releases a new Android GKI (Generic Kernel Image) version, the
KMI (Kernel Module Interface) surface may change: symbols are added, removed,
or have their signatures modified. This guide covers the full update workflow.

## GKI Release Cadence

Android GKI follows a `androidNN-X.Y` naming scheme:

| Branch | Kernel | Android | Example |
|--------|--------|---------|---------|
| `common-android14-6.1` | 6.1 | 14 | Pixel 8 |
| `common-android15-6.6` | 6.6 | 15 | Pixel 10 |
| `common-android16-6.12` | 6.12 | 16 | Future |

Each release has a frozen KMI (defined by `abi_gki_aarch64` symbol lists in
the kernel source). Between major releases, the KMI is stable. DriverHub
must be updated when targeting a new KMI generation.

## Update Workflow Overview

```
1. Obtain new GKI artifacts (kernel, modules, symvers, headers)
2. Run symbol coverage analysis (what changed?)
3. Update shim stubs for new/changed symbols
4. Update symbol registry entries
5. Build and test with new modules
6. Update documentation and version references
```

## Step 1: Obtain New GKI Artifacts

### From the Android kernel build

```shell
# Clone or update the kernel source
repo init -u https://android.googlesource.com/kernel/manifest \
  -b common-android16-6.12
repo sync -j$(nproc)

# Build GKI
BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh

# Key artifacts in out/android16-6.12/dist/:
#   vmlinux.symvers    — all exported symbols with CRCs
#   Module.symvers     — module-exported symbols
#   *.ko               — built-in modules
#   System.map         — kernel symbol map
```

### From a factory image

```shell
# Download a factory image for the target device
# (e.g., from https://developers.google.com/android/images)

cd tools/extract-factory
cargo build --release

./target/release/extract-factory \
  --zip /path/to/factory-image.zip \
  --output /tmp/new-gki \
  --list-symbols
```

### From CI / prebuilt

Google publishes prebuilt GKI kernels. Check:
- `https://ci.android.com/builds/branches/aosp-kernel-common-android16-6.12`
- Look for `kernel_aarch64` builds with `vmlinux.symvers`

### Obtain kernel headers

Headers are needed for generating stubs with correct function signatures:

```shell
# From the kernel build
BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh headers_install

# Or download the GKI headers package
# Headers end up in out/android16-6.12/dist/kernel-headers/
```

## Step 2: Symbol Coverage Analysis

### Compare old vs. new symvers

```shell
# Diff the symbol lists
diff <(cut -f2 old-vmlinux.symvers | sort) \
     <(cut -f2 new-vmlinux.symvers | sort) \
  | head -50

# Count changes
comm -23 <(cut -f2 old-vmlinux.symvers | sort) \
         <(cut -f2 new-vmlinux.symvers | sort) | wc -l  # Removed
comm -13 <(cut -f2 old-vmlinux.symvers | sort) \
         <(cut -f2 new-vmlinux.symvers | sort) | wc -l  # Added
```

### Run DriverHub's coverage tool

```shell
# Full coverage report against the new symvers
./tools/symvers_parser.py \
  --symvers /tmp/new-gki/vmlinux.symvers \
  --registry src/symbols/symbol_registry.cc \
  --generate-stubs

# Check coverage per module
./tools/symvers_parser.py \
  --symvers /tmp/new-gki/vmlinux.symvers \
  --registry src/symbols/symbol_registry.cc \
  --module-dir /tmp/new-gki/modules/ \
  --top 50

# Categorize missing symbols by subsystem
./tools/symvers_parser.py \
  --symvers /tmp/new-gki/vmlinux.symvers \
  --registry src/symbols/symbol_registry.cc \
  --category
```

### Check KMI symbol list (stable ABI)

The KMI symbol list defines the frozen ABI. Symbols on this list must be
provided exactly:

```shell
# Download the KMI symbol list from the kernel source
# Located at: common/android/abi_gki_aarch64

./tools/symvers_parser.py \
  --kmi-symbols /path/to/abi_gki_aarch64 \
  --registry src/symbols/symbol_registry.cc
```

### Identify critical gaps

Focus on symbols that block the most modules:

```
=== Most-needed missing symbols (top 10) ===
  Symbol                            Modules needing it
  ----------------------------------------------------------------
  new_kernel_function                    28  (GKI)
  another_new_api                        15  (GKI)
```

## Step 3: Update Shim Stubs

### Auto-generate stubs for new symbols

```shell
./tools/generate_stubs.py \
  --headers /tmp/new-gki/kernel-headers/include \
  --symvers /tmp/new-gki/vmlinux.symvers \
  --registry src/symbols/symbol_registry.cc \
  --modules /tmp/new-gki/modules/ \
  --output-stubs src/shim/kernel/gki_stubs_new.cc \
  --output-registry /tmp/new_register_entries.txt \
  --min-modules 2
```

This generates:
- `gki_stubs_new.cc`: Stub implementations (no-op / zero-return) for
  missing symbols, with correct signatures from kernel headers
- `new_register_entries.txt`: `REGISTER_SYMBOL()` entries to add to the
  symbol registry

### Review generated stubs

**Not all stubs should be no-ops.** Review the output and categorize:

| Category | Action | Example |
|----------|--------|---------|
| Safe no-op | Keep as-is | `debugfs_create_dir` (debug-only) |
| Needs real impl | Write shim | `new_alloc_func` (memory management) |
| Renamed function | Alias to existing shim | `kmalloc_noprof` → `kmalloc` |
| Removed function | Delete old shim | `deprecated_api` |
| Signature changed | Update existing shim | `old_func(a)` → `old_func(a, b)` |

### Handle renamed symbols

GKI occasionally renames internal functions (especially `_noprof` variants
for memory profiling). Add aliases:

```cpp
// src/shim/kernel/memory.cc
extern "C" void* kmalloc(size_t size, gfp_t flags) {
    return malloc(size);
}

// GKI 6.12 adds profiling-free variants
extern "C" void* kmalloc_noprof(size_t size, gfp_t flags) {
    return kmalloc(size, flags);
}
```

### Handle signature changes

If a function's signature changed, update the shim to match the new
signature while preserving behavior:

```cpp
// Old (GKI 6.6):
// extern "C" int old_api(struct device *dev);

// New (GKI 6.12):
// extern "C" int old_api(struct device *dev, unsigned int flags);
extern "C" int old_api(struct device* dev, unsigned int flags) {
    (void)flags;  // New parameter, not used by our shim
    return 0;
}
```

### Handle removed symbols

If a symbol was removed from the new GKI, check if any modules still
reference it. If not, remove the shim and registry entry. If yes, keep
the shim for backward compatibility.

## Step 4: Update Symbol Registry

### Add new entries

Merge the generated `REGISTER_SYMBOL()` entries into
`src/symbols/symbol_registry.cc`:

```cpp
void SymbolRegistry::RegisterKmiSymbols() {
    // Existing entries...
    REGISTER_SYMBOL(kmalloc);
    REGISTER_SYMBOL(kfree);

    // New entries for GKI 6.12
    REGISTER_SYMBOL(kmalloc_noprof);
    REGISTER_SYMBOL(new_kernel_function);
}
```

### Remove obsolete entries

If a symbol was removed from GKI and no modules need it, remove its
`REGISTER_SYMBOL()` entry and shim implementation.

### Update CRC values (optional)

`vmlinux.symvers` includes CRC values for each symbol. DriverHub currently
does not enforce CRC checking (since we're not running a real Linux kernel),
but tracking CRCs helps detect silent ABI changes:

```shell
# Extract CRC changes
diff <(sort old-vmlinux.symvers) <(sort new-vmlinux.symvers) | \
  grep -E '^[<>]' | head -20
```

## Step 5: Build and Test

### Rebuild DriverHub

```shell
# Host build (quick iteration)
make clean && make && make test-all

# Fuchsia build
fx build //src/drivers/driverhub
fx test driverhub-tests
```

### Test with new modules

```shell
# Bulk-test all new GKI modules
./tools/test-all-modules.sh /tmp/new-gki/modules/ ./build/driverhub

# Test specific critical modules
echo | ./driverhub /tmp/new-gki/modules/bluetooth.ko
echo | ./driverhub /tmp/new-gki/modules/cfg80211.ko
echo | ./driverhub /tmp/new-gki/modules/rfkill.ko
```

### Cross-compile and test on Fuchsia QEMU

```shell
# Follow docs/fuchsia-env-setup.md for environment setup, then:

# Build for Fuchsia x86_64
$CXX --target=x86_64-unknown-fuchsia --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 ... -o /tmp/driverhub-fuchsia ...

# Create ZBI with new module
$ZBI -o /tmp/fuchsia-test.zbi \
  $ORIG_ZBI \
  --entry bin/driverhub=/tmp/driverhub-fuchsia \
  --entry data/modules/test.ko=/tmp/new-gki/modules/test.ko \
  -T CMDLINE <(echo 'zircon.autorun.boot=/boot/bin/driverhub+/boot/data/modules/test.ko')

# Boot QEMU
timeout 90 $QEMU -kernel $KERNEL -initrd /tmp/fuchsia-test.zbi \
  -m 4096 -smp 2 -nographic -no-reboot -machine q35 \
  -cpu Haswell,+smap,-check,-fsgsbase -serial stdio -monitor none -net none
```

## Step 6: Update Documentation

### Update version references

- `docs/gki-coverage-report.txt` — regenerate with new symvers
- `docs/fuchsia-env-setup.md` — update SDK/toolchain versions if needed
- `CLAUDE.md` — update if build commands changed

### Regenerate coverage report

```shell
./tools/symvers_parser.py \
  --symvers /tmp/new-gki/vmlinux.symvers \
  --registry src/symbols/symbol_registry.cc \
  --module-dir /tmp/new-gki/modules/ \
  --top 50 \
  > docs/gki-coverage-report.txt
```

### Update the GKI version constant

If DriverHub tracks a GKI version string internally, update it:

```cpp
// src/loader/module_loader.cc or similar
constexpr const char* kTargetGkiVersion = "android16-6.12";
```

## Step 7: Commit and Review

Follow the project's commit conventions:

```shell
# Stage changes
git add src/shim/kernel/gki_stubs.cc
git add src/symbols/symbol_registry.cc
git add docs/gki-coverage-report.txt

# Commit
git commit -m "feat(shim): update KMI shims for GKI android16-6.12

- Add 45 new symbol stubs for kernel 6.12 KMI surface
- Update kmalloc/kfree to handle _noprof variants
- Remove 3 deprecated symbols from 6.6
- Coverage: 520/9200 symbols (5.7%), 8/96 modules fully loadable"
```

## Common Pitfalls

### Symbol renamed but not removed

GKI sometimes renames a function but keeps the old name as a `static inline`
wrapper in a header. The `.ko` modules may reference either name depending
on when they were built. Provide both:

```cpp
extern "C" void* kmalloc(size_t s, gfp_t f) { return malloc(s); }
extern "C" void* kmalloc_noprof(size_t s, gfp_t f) { return kmalloc(s, f); }
```

### Struct layout changes

If a kernel struct's layout changes between versions, `.ko` modules compiled
against the old layout will access wrong field offsets. This is the hardest
class of breakage. Mitigations:

1. **Use accessor functions**: Shim the struct's accessor functions rather
   than exposing raw struct layout.
2. **Version-specific headers**: Ship GKI version-specific shim headers
   under `src/shim/include/abi/`.
3. **Verify with test modules**: Build test `.ko` files against both old
   and new headers, load both, and verify behavior.

### GPL-only symbols

Some GKI symbols are `EXPORT_SYMBOL_GPL`. DriverHub shims these without
restriction (we're not a Linux kernel, so GPL export restrictions don't
apply to our shim layer). However, track which symbols are GPL-only for
documentation:

```shell
# List GPL-only symbols we provide
./tools/symvers_parser.py --symvers vmlinux.symvers --registry src/symbols/symbol_registry.cc \
  | grep EXPORT_SYMBOL_GPL
```

### Module vermagic mismatch

`.ko` modules embed a `vermagic` string (e.g., `6.6.30-android15-6-...`).
DriverHub skips vermagic validation since it's not a real kernel, but
you should track which GKI version your modules were built against to
catch struct layout issues early.

## Quick Reference: Update Checklist

```
[ ] Download new vmlinux.symvers and kernel headers
[ ] Run symvers_parser.py — note new/removed/changed symbols
[ ] Run generate_stubs.py — auto-generate stub code
[ ] Review stubs: categorize as no-op / real-impl / alias / remove
[ ] Implement real shims for critical new APIs
[ ] Add aliases for renamed functions
[ ] Update signature for changed functions
[ ] Remove shims for deleted symbols (if no modules need them)
[ ] Add REGISTER_SYMBOL() entries for all new symbols
[ ] make clean && make && make test-all
[ ] Bulk-test with new .ko modules
[ ] Regenerate docs/gki-coverage-report.txt
[ ] Commit with descriptive message
```
