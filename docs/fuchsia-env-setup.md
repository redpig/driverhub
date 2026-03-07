# Fuchsia Cross-Compilation & QEMU Environment Setup

This document describes how to set up the Fuchsia development environment for
cross-compiling DriverHub and booting it on Fuchsia QEMU. These steps were
validated on a Linux x86_64 host.

## Prerequisites

```shell
# Required host tools
apt-get update && apt-get install -y curl unzip
```

## 1. Fuchsia IDK (SDK)

The Fuchsia IDK provides sysroot headers, FIDL libraries, and host tools
(ffx, zbi, qemu).

```shell
# Get the latest SDK version
SDK_VERSION=$(curl -s https://storage.googleapis.com/fuchsia/development/LATEST_LINUX)
echo "SDK version: $SDK_VERSION"
# As of 2026-03-07: 31.20260306.7.1

# Download and extract the core IDK
mkdir -p third_party/fuchsia-idk
curl -L -o third_party/fuchsia-idk/core.tar.gz \
  "https://storage.googleapis.com/fuchsia/development/${SDK_VERSION}/sdk/linux-amd64/core.tar.gz"
cd third_party/fuchsia-idk && tar xzf core.tar.gz --no-same-owner && cd ../..

# Verify
ls third_party/fuchsia-idk/arch/x64/sysroot/include/zircon/types.h
```

**Key paths after extraction:**

| Path | Contents |
|------|----------|
| `arch/x64/sysroot/` | x86_64 sysroot (headers + libs) |
| `arch/x64/lib/` | Fuchsia shared libraries (libfdio.so, etc.) |
| `arch/x64/sysroot/lib/libzircon.so` | Zircon syscall stub library |
| `tools/x64/zbi` | ZBI image manipulation tool |
| `tools/x64/ffx` | Fuchsia developer tool |
| `tools/x64/qemu_internal/bin/` | Bundled QEMU binaries |
| `meta/manifest.json` | IDK metadata (version, arch targets) |

## 2. Fuchsia Clang Toolchain

The Fuchsia-flavored Clang includes the C++ runtime libraries
(`libc++.so`, `libc++abi.so`, `libunwind.so`) targeting Fuchsia.

```shell
# Download from CIPD (Chrome Infrastructure Package Deployment)
curl -sL "https://chrome-infra-packages.appspot.com/dl/fuchsia/third_party/clang/linux-amd64/+/latest" \
  -o /tmp/fuchsia-clang.zip

# Extract into third_party/clang
mkdir -p third_party/clang
cd third_party/clang && unzip -qo /tmp/fuchsia-clang.zip && cd ../..

# Verify
third_party/clang/bin/clang++ --version
# Expected: Fuchsia clang version 23.0.0git (...)
```

**Key paths after extraction:**

| Path | Contents |
|------|----------|
| `bin/clang++` | Cross-compiler (supports `--target=x86_64-unknown-fuchsia`) |
| `lib/x86_64-unknown-fuchsia/libc++.so` | Real C++ runtime for Fuchsia x86_64 |
| `lib/x86_64-unknown-fuchsia/libc++abi.so` | C++ ABI library |
| `lib/x86_64-unknown-fuchsia/libunwind.so` | Unwinder library |

## 3. Fuchsia Product Bundle (Kernel + ZBI)

The product bundle provides the Zircon kernel, base ZBI image, and other
boot artifacts needed to run Fuchsia in QEMU.

```shell
# Disable ffx analytics
third_party/fuchsia-idk/tools/x64/ffx config analytics disable 2>/dev/null

# Find the product bundle download URL from the SDK's product_bundles.json
SDK_VERSION="31.20260306.7.1"  # or use $(cat third_party/fuchsia-idk/meta/manifest.json | python3 -c "import json,sys; print(json.load(sys.stdin)['id'])")
PB_JSON=$(curl -s "https://storage.googleapis.com/fuchsia/development/${SDK_VERSION}/product_bundles.json")

# Get the transfer manifest URL for minimal.x64
TRANSFER_URL=$(echo "$PB_JSON" | python3 -c "
import json, sys
data = json.load(sys.stdin)
for pb in data:
    if pb['name'] == 'minimal.x64':
        print(pb['transfer_manifest_url'])
        break
")
echo "Transfer URL: $TRANSFER_URL"
```

### Direct Download (without ffx)

The product bundle base URL for `minimal.x64` follows this pattern:
```
https://storage.googleapis.com/fuchsia-public-artifacts-release/builds/<BUILD_ID>/product_bundles/minimal.x64/
```

Download the essential boot files:

```shell
# Extract the base URL from the transfer manifest URL
# Example BUILD_ID: 8688041560696143137 (from SDK 31.20260306.7.1)
BASE="https://storage.googleapis.com/fuchsia-public-artifacts-release/builds/8688041560696143137/product_bundles/minimal.x64"

mkdir -p /tmp/fuchsia-pb/system_a

# Download boot images (these are the minimum needed for QEMU boot)
curl -s -o /tmp/fuchsia-pb/system_a/fuchsia.zbi "$BASE/system_a/fuchsia.zbi"
curl -s -o /tmp/fuchsia-pb/system_a/linux-x86-boot-shim.bin "$BASE/system_a/linux-x86-boot-shim.bin"
curl -s -o /tmp/fuchsia-pb/system_a/fuchsia.vbmeta "$BASE/system_a/fuchsia.vbmeta"
curl -s -o /tmp/fuchsia-pb/system_a/fxfs.sparse.blk "$BASE/system_a/fxfs.sparse.blk"
curl -s -o /tmp/fuchsia-pb/product_bundle.json "$BASE/product_bundle.json"

# Verify
ls -lh /tmp/fuchsia-pb/system_a/
# fuchsia.zbi              ~23M  (Zircon Boot Image with bootfs)
# linux-x86-boot-shim.bin  ~378K (multiboot shim for QEMU)
# fxfs.sparse.blk          ~67M  (Fxfs data partition)
```

**Save the original ZBI** (before modifying):
```shell
cp /tmp/fuchsia-pb/system_a/fuchsia.zbi /tmp/fuchsia-pb/system_a/fuchsia-orig.zbi
```

### Optional: Full Product Bundle Download

For a complete product bundle (includes repository, blobs, virtual device
configs), you can also download:

```shell
# Virtual device configurations (for ffx emu)
mkdir -p /tmp/fuchsia-pb/virtual_devices
for f in x64-emu-recommended.json x64-emu-min.json x64-emu-large.json; do
    curl -s -o "/tmp/fuchsia-pb/virtual_devices/$f" "$BASE/virtual_devices/$f"
done

# Repository metadata and blobs (needed for package resolution at runtime)
# These are large (~500+ blob files) and only needed if you want ffx emu
# or package serving. For bootfs-only testing, skip this.
```

## 4. Cross-Compiling DriverHub for Fuchsia

### Compile Object Files

```shell
CXX=third_party/clang/bin/clang++
CC=third_party/clang/bin/clang
SYSROOT=third_party/fuchsia-idk/arch/x64/sysroot
OBJDIR=/tmp/dfv2-poc-obj

CXXFLAGS="--target=x86_64-unknown-fuchsia --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 -std=c++17 -Wall -g \
  -I. -Isrc/shim/include -fno-pie -fPIC -D__Fuchsia__"

mkdir -p $OBJDIR

# Core sources
SOURCES=(
  src/bus_driver/bus_driver.cc
  src/module_node/module_node.cc
  src/loader/module_loader.cc
  src/loader/dependency_sort.cc
  src/loader/memory_allocator.cc
  src/loader/mmap_allocator.cc
  src/loader/modinfo_parser.cc
  src/fuchsia/module_enumerator.cc
  src/fuchsia/vmo_module_loader.cc
  src/fuchsia/resource_provider.cc
  src/symbols/symbol_registry.cc
  src/dt/dt_provider.cc
  # Kernel shims
  src/shim/kernel/memory.cc
  src/shim/kernel/print.cc
  src/shim/kernel/sync.cc
  src/shim/kernel/module.cc
  src/shim/kernel/device.cc
  src/shim/kernel/platform.cc
  src/shim/kernel/time.cc
  src/shim/kernel/timer.cc
  src/shim/kernel/workqueue.cc
  src/shim/kernel/irq.cc
  src/shim/kernel/io.cc
  src/shim/kernel/bug.cc
  src/shim/kernel/clk.cc
  # Subsystem shims
  src/shim/subsystem/rtc.cc
  src/shim/subsystem/i2c.cc
  src/shim/subsystem/spi.cc
  src/shim/subsystem/gpio.cc
  src/shim/subsystem/usb.cc
  src/shim/subsystem/drm.cc
  src/shim/subsystem/input.cc
  src/shim/subsystem/cfg80211.cc
  src/shim/subsystem/fs.cc
  src/shim/subsystem/block.cc
  src/shim/subsystem/scsi.cc
)

for src in "${SOURCES[@]}"; do
  obj=$OBJDIR/$(basename "${src%.cc}.o")
  $CXX $CXXFLAGS -c -o "$obj" "$src"
done

# Compile the demo main
$CXX $CXXFLAGS -c -o $OBJDIR/dfv2_dt_demo.o src/fuchsia/dfv2_dt_demo.cc
```

### Link the Binary

**Critical**: Library path ordering matters. The Fuchsia clang runtime path
must come *before* the sysroot lib path. Otherwise the linker finds the
sysroot's stub `libc++.so` instead of the real one from the clang toolchain.

```shell
CXX=third_party/clang/bin/clang++
SYSROOT=third_party/fuchsia-idk/arch/x64/sysroot
CLANG_RTLIB=third_party/clang/lib/x86_64-unknown-fuchsia
IDK_LIBS=third_party/fuchsia-idk/arch/x64/lib
OBJDIR=/tmp/dfv2-poc-obj

$CXX --target=x86_64-unknown-fuchsia --sysroot=$SYSROOT -ffuchsia-api-level=30 \
  -L$CLANG_RTLIB \
  -L$IDK_LIBS \
  -L$SYSROOT/lib \
  -o /tmp/dfv2-dt-demo $OBJDIR/*.o \
  -lc++ -lc++abi -lunwind -lfdio -lzircon -lpthread

# Verify
file /tmp/dfv2-dt-demo
# Expected: ELF 64-bit LSB pie executable, x86-64, ... dynamically linked, interpreter ld.so.1
```

**Common linking pitfalls:**

| Error | Cause | Fix |
|-------|-------|-----|
| `undefined reference to zx_vmo_create` | Missing libzircon | Add `-L$SYSROOT/lib -lzircon` |
| `undefined reference to operator new` | Wrong libc++ found | Put `-L$CLANG_RTLIB` *before* `-L$SYSROOT/lib` |
| `undefined reference to __cxa_guard_acquire` | Missing libc++abi | Add `-lc++ -lc++abi -lunwind` explicitly |

## 5. Building a Bootable ZBI

The ZBI tool adds files to the BOOTFS section of the Zircon Boot Image.
Files in BOOTFS are available at `/boot/` in Fuchsia.

```shell
ZBI=third_party/fuchsia-idk/tools/x64/zbi
ORIG_ZBI=/tmp/fuchsia-pb/system_a/fuchsia-orig.zbi

# Add the binary and .ko module to BOOTFS, plus an autorun command
$ZBI -o /tmp/fuchsia-dfv2.zbi \
  $ORIG_ZBI \
  --entry bin/dfv2-dt-demo=/tmp/dfv2-dt-demo \
  --entry data/modules/rtc-test.ko=third_party/linux/rtc-test/rtc-test.ko \
  -T CMDLINE <(echo 'zircon.autorun.boot=/boot/bin/dfv2-dt-demo+/boot/data/modules/rtc-test.ko')

# Verify contents
$ZBI --list --verbose /tmp/fuchsia-dfv2.zbi | grep -E 'dfv2|rtc|autorun'
# Expected:
#   bin/dfv2-dt-demo
#   data/modules/rtc-test.ko
#   zircon.autorun.boot=/boot/bin/dfv2-dt-demo+/boot/data/modules/rtc-test.ko
```

**Notes:**
- `--entry <bootfs-path>=<host-path>` adds a file to BOOTFS
- `-T CMDLINE <file>` appends kernel command-line arguments
- The `+` in the autorun value is the Fuchsia boot-arg space separator
- Files appear at `/boot/<bootfs-path>` in the running system

## 6. Booting Fuchsia QEMU

```shell
QEMU=third_party/fuchsia-idk/tools/x64/qemu_internal/bin/qemu-system-x86_64
KERNEL=/tmp/fuchsia-pb/system_a/linux-x86-boot-shim.bin
ZBI=/tmp/fuchsia-dfv2.zbi

timeout 90 $QEMU \
  -kernel $KERNEL \
  -initrd $ZBI \
  -m 4096 \
  -smp 2 \
  -nographic \
  -no-reboot \
  -machine q35 \
  -cpu Haswell,+smap,-check,-fsgsbase \
  -serial stdio \
  -monitor none \
  -net none \
  2>&1 | tee /tmp/qemu-output.log
```

**QEMU flags explained:**

| Flag | Purpose |
|------|---------|
| `-kernel` | Linux boot shim (multiboot entry → Zircon) |
| `-initrd` | ZBI image (kernel + bootfs + cmdline) |
| `-machine q35` | Q35 chipset (PCIe, AHCI) |
| `-cpu Haswell,+smap,-check,-fsgsbase` | CPU model compatible with Fuchsia |
| `-nographic -serial stdio` | Serial console on stdout |
| `-no-reboot` | Exit on kernel panic instead of reboot |

### Expected Output

With `zircon.autorun.boot` configured, the console-launcher automatically
runs the demo after Fuchsia boots:

```
[console-launcher] INFO: starting 'autorun:boot': /boot/bin/dfv2-dt-demo /boot/data/modules/rtc-test.ko

============================================================
  DriverHub DFv2 Device-Tree Proof of Concept
============================================================

--- Phase 1: Device Tree Configuration ---
[dfv2-poc] DTB built: 285 bytes
[dfv2-poc] Device tree entries:
  [0] compatible='test,rtc-test' module='rtc_test.ko' resources=2

--- Phase 2: DFv2 Bus Driver Init ---
driverhub: bus driver ready (377 KMI symbols registered)

--- Phase 3: Load Module from DT ---
driverhub: loaded rtc-test (7144 bytes, 48 symbols)
driverhub: rtc: registered rtc0, rtc1, rtc2

--- Phase 5: Exercise RTC Ops ---
[dfv2-poc] read_time() = 0
[dfv2-poc] set_time(2026-03-07 12:00:00) = 0
[dfv2-poc] Readback: 2026-03-07 12:00:00

--- Phase 6: DFv2 Shutdown ---

============================================================
  DFv2 Device-Tree PoC Results
============================================================
  DT parsing:        PASS (1 entries)
  Module loading:    PASS
  RTC registration:  PASS (3 devices)
  RTC read_time:     PASS
  RTC set_time:      PASS
  RTC readback:      PASS
============================================================
```

### Running Commands Interactively

If you omit the autorun CMDLINE, you get an interactive shell at boot
(because `console.shell=true` is in the base ZBI). From the `$` prompt:

```
$ /boot/bin/dfv2-dt-demo /boot/data/modules/rtc-test.ko
```

## 7. Quick Reference

### Version Matrix (validated 2026-03-07)

| Component | Version | Source |
|-----------|---------|--------|
| Fuchsia IDK | 31.20260306.7.1 | GCS: `fuchsia/development/` |
| Fuchsia Clang | 23.0.0git (3fc27dc) | CIPD: `fuchsia/third_party/clang/linux-amd64` |
| Product Bundle | minimal.x64 31.20260306.7.1 | GCS: `fuchsia-public-artifacts-release/` |
| QEMU | 9.2.0 (bundled in IDK) | IDK `tools/x64/qemu_internal/` |

### Environment Variables

```shell
export DRIVERHUB=$PWD
export IDK=$DRIVERHUB/third_party/fuchsia-idk
export CXX=$DRIVERHUB/third_party/clang/bin/clang++
export SYSROOT=$IDK/arch/x64/sysroot
export CLANG_RTLIB=$DRIVERHUB/third_party/clang/lib/x86_64-unknown-fuchsia
export ZBI=$IDK/tools/x64/zbi
export QEMU=$IDK/tools/x64/qemu_internal/bin/qemu-system-x86_64
```

### One-Liner: Full Rebuild and Boot

```shell
# Cross-compile, build ZBI, and boot (assumes environment is set up)
make -j$(nproc) && \
  $CXX --target=x86_64-unknown-fuchsia --sysroot=$SYSROOT -ffuchsia-api-level=30 \
    -L$CLANG_RTLIB -L$IDK/arch/x64/lib -L$SYSROOT/lib \
    -o /tmp/dfv2-dt-demo /tmp/dfv2-poc-obj/*.o \
    -lc++ -lc++abi -lunwind -lfdio -lzircon -lpthread && \
  $ZBI -o /tmp/fuchsia-dfv2.zbi /tmp/fuchsia-pb/system_a/fuchsia-orig.zbi \
    --entry bin/dfv2-dt-demo=/tmp/dfv2-dt-demo \
    --entry data/modules/rtc-test.ko=third_party/linux/rtc-test/rtc-test.ko \
    -T CMDLINE <(echo 'zircon.autorun.boot=/boot/bin/dfv2-dt-demo+/boot/data/modules/rtc-test.ko') && \
  timeout 90 $QEMU -kernel /tmp/fuchsia-pb/system_a/linux-x86-boot-shim.bin \
    -initrd /tmp/fuchsia-dfv2.zbi -m 4096 -smp 2 -nographic -no-reboot \
    -machine q35 -cpu Haswell,+smap,-check,-fsgsbase -serial stdio -monitor none -net none
```
