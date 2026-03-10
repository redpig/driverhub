#!/bin/bash
# scripts/test-rfkill-qemu.sh
#
# End-to-end test: load the real GKI rfkill.ko module in QEMU, start the
# rfkill IPC server, and exercise rfkillctl-style commands over Unix socket.
#
# Validates on aarch64:
#   - ELF loading + ARM64 relocations for rfkill.ko
#   - KMI symbol resolution (all 545+ symbols)
#   - rfkill.ko init_module (class_register, misc device, LED triggers)
#   - module's own rfkill_alloc / rfkill_register for test radios
#   - rfkill IPC server (LIST, BLOCK, UNBLOCK, BLOCKALL, UNBLOCKALL)
#   - Error handling for invalid indices
#
# Supports both x86_64 and aarch64 architectures.
#
# Prerequisites:
#   - Fuchsia IDK extracted at third_party/fuchsia-idk/
#   - Fuchsia Clang at third_party/clang/
#   - Fuchsia product bundle (see docs/fuchsia-env-setup.md)
#   - test_modules/rfkill.ko (run scripts/fetch-gki-modules.sh)
#
# Usage:
#   ./scripts/test-rfkill-qemu.sh          # aarch64 (default, matches .ko arch)
#   ./scripts/test-rfkill-qemu.sh arm64    # explicit aarch64
#   ./scripts/test-rfkill-qemu.sh x64      # x86_64 (requires x64-compiled .ko)
#
# Exits 0 if all 14 tests pass in the QEMU serial output.

set -euo pipefail

DRIVERHUB="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DRIVERHUB"

# ---------------------------------------------------------------------------
# Architecture selection (default arm64 since rfkill.ko is aarch64)
# ---------------------------------------------------------------------------
ARCH="${1:-arm64}"

case "$ARCH" in
  x64|x86_64|x86)
    ARCH=x64
    TARGET_TRIPLE=x86_64-unknown-fuchsia
    IDK_ARCH=x64
    CLANG_ARCH=x86_64-unknown-fuchsia
    QEMU_BIN=qemu-system-x86_64
    QEMU_MACHINE="-machine q35 -cpu Haswell,+smap,-check,-fsgsbase"
    QEMU_KERNEL_FLAG="-kernel"
    PB_DIR=/tmp/fuchsia-pb
    BOOT_KERNEL=linux-x86-boot-shim.bin
    NEEDS_KERNEL=true
    ;;
  arm64|aarch64)
    ARCH=arm64
    TARGET_TRIPLE=aarch64-unknown-fuchsia
    IDK_ARCH=arm64
    CLANG_ARCH=aarch64-unknown-fuchsia
    QEMU_BIN=qemu-system-aarch64
    QEMU_MACHINE="-machine virt,gic-version=3,virtualization=on -cpu cortex-a53"
    QEMU_KERNEL_FLAG="-kernel"
    PB_DIR=/tmp/fuchsia-pb-arm64
    BOOT_KERNEL=linux-arm64-boot-shim.bin
    NEEDS_KERNEL=true
    ;;
  *)
    echo "Usage: $0 [x64|arm64]"
    exit 1
    ;;
esac

echo "=== DriverHub RFkill QEMU Test ($ARCH) ==="
echo ""

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
CXX=third_party/clang/bin/clang++
CC=third_party/clang/bin/clang
IDK=third_party/fuchsia-idk
SYSROOT=$IDK/arch/$IDK_ARCH/sysroot
CLANG_RTLIB=third_party/clang/lib/$CLANG_ARCH
IDK_LIBS=$IDK/arch/$IDK_ARCH/lib
ZBI_TOOL=$IDK/tools/x64/zbi
QEMU=$IDK/tools/x64/qemu_internal/bin/$QEMU_BIN

ORIG_ZBI=$PB_DIR/system_a/fuchsia-orig.zbi
RFKILL_KO=test_modules/rfkill.ko

OBJDIR=/tmp/rfkill-qemu-obj-$ARCH
OUTPUT_ZBI=/tmp/fuchsia-rfkill-$ARCH.zbi
OUTPUT_BIN=/tmp/rfkill-qemu-demo-$ARCH
QEMU_LOG=/tmp/qemu-rfkill-output-$ARCH.log

# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------
echo "=== Preflight checks ==="

PREFLIGHT_FILES=("$CXX" "$CC" "$ZBI_TOOL" "$QEMU" "$ORIG_ZBI" "$RFKILL_KO")
if [ "$NEEDS_KERNEL" = true ]; then
  KERNEL_PATH="$PB_DIR/system_a/$BOOT_KERNEL"
  PREFLIGHT_FILES+=("$KERNEL_PATH")
fi

for f in "${PREFLIGHT_FILES[@]}"; do
  if [ ! -f "$f" ]; then
    echo "ERROR: missing $f"
    if [[ "$f" == *rfkill.ko ]]; then
      echo "  Run: ./scripts/fetch-gki-modules.sh"
    elif [[ "$f" == *fuchsia-pb* ]]; then
      echo "  See docs/fuchsia-env-setup.md for setup instructions."
    fi
    exit 1
  fi
done

echo "  Architecture:    $ARCH ($TARGET_TRIPLE)"
echo "  Toolchain:       $($CXX --version 2>&1 | head -1)"
echo "  Module:          $RFKILL_KO ($(file -b "$RFKILL_KO" | head -c 60))"
echo "  ZBI tool:        $ZBI_TOOL"
echo "  QEMU:            $QEMU"
echo "  Original ZBI:    $ORIG_ZBI"

# ---------------------------------------------------------------------------
# Step 1: Build IDK include flags
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 1: Collecting IDK include paths ==="

IDK_INCS=""
for d in $(find "$IDK/pkg" -name include -type d 2>/dev/null); do
  IDK_INCS="$IDK_INCS -I$d"
done

CXXFLAGS="--target=$TARGET_TRIPLE --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 -std=c++17 -Wall -g \
  -I. -Isrc/shim/include -Irust/driverhub-core/include -fno-pie -fPIC -D__Fuchsia__ $IDK_INCS"

echo "  Found $(echo $IDK_INCS | wc -w) IDK package include dirs"

# ---------------------------------------------------------------------------
# Step 1b: Build Rust core library for target architecture
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 1b: Building Rust core library for $TARGET_TRIPLE ==="

RUST_TARGET_DIR=rust/driverhub-core/target/$TARGET_TRIPLE/release
RUST_LIB=$RUST_TARGET_DIR/libdriverhub_core.a

(cd rust/driverhub-core && cargo build --release --target "$TARGET_TRIPLE" 2>&1)

if [ ! -f "$RUST_LIB" ]; then
  echo "ERROR: Rust library not found at $RUST_LIB"
  exit 1
fi

echo "  Built: $RUST_LIB ($(du -h "$RUST_LIB" | cut -f1))"

# ---------------------------------------------------------------------------
# Step 2: Cross-compile all sources
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 2: Cross-compiling for $TARGET_TRIPLE ==="

mkdir -p "$OBJDIR"
rm -f "$OBJDIR"/*.o

SOURCES=(
  src/bus_driver/bus_driver.cc
  src/module_node/module_node.cc
  src/loader/module_loader.cc
  src/loader/module_executor.cc
  src/loader/dependency_sort.cc
  src/loader/memory_allocator.cc
  src/loader/mmap_allocator.cc
  src/loader/modinfo_parser.cc
  src/fuchsia/module_enumerator.cc
  src/fuchsia/vmo_module_loader.cc
  src/fuchsia/resource_provider.cc
  src/fuchsia/service_bridge.cc
  src/symbols/symbol_registry.cc
  src/dt/dt_provider.cc
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
  src/shim/subsystem/thermal.cc
  src/shim/subsystem/iio.cc
  src/shim/subsystem/pinctrl.cc
  src/shim/subsystem/watchdog.cc
  src/shim/subsystem/nvmem.cc
  src/shim/subsystem/power_supply.cc
  src/shim/subsystem/pwm.cc
  src/shim/subsystem/led.cc
  src/shim/subsystem/kunit.cc
  src/shim/subsystem/net.cc
  src/shim/subsystem/vfs_service.cc
  src/shim/kernel/rfkill_stubs.cc
  src/shim/kernel/libc_stubs.cc
  src/shim/kernel/gki_stubs.cc
)

COMPILED=0
for src in "${SOURCES[@]}"; do
  obj=$OBJDIR/$(basename "${src%.cc}.o")
  $CXX $CXXFLAGS -c -o "$obj" "$src" 2>&1
  COMPILED=$((COMPILED + 1))
done

echo "  Compiled $COMPILED core sources"

# Compile the Zircon zx library from IDK sources.
ZX_FLAGS="--target=$TARGET_TRIPLE --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 -std=c++17 -g -fPIC $IDK_INCS"

ZX_COUNT=0
for src in "$IDK/pkg/zx"/*.cc; do
  obj=$OBJDIR/zx_$(basename "${src%.cc}.o")
  $CXX $ZX_FLAGS -c -o "$obj" "$src" 2>&1
  ZX_COUNT=$((ZX_COUNT + 1))
done

echo "  Compiled $ZX_COUNT zx library sources"

# Compile the rfkill QEMU demo binary.
$CXX $CXXFLAGS -c -o "$OBJDIR/rfkill_qemu_demo.o" src/fuchsia/rfkill_qemu_demo.cc 2>&1
echo "  Compiled rfkill_qemu_demo.cc"

# ---------------------------------------------------------------------------
# Step 3: Link
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 3: Linking ==="

$CXX --target="$TARGET_TRIPLE" --sysroot="$SYSROOT" -ffuchsia-api-level=30 \
  -L"$CLANG_RTLIB" \
  -L"$IDK_LIBS" \
  -L"$SYSROOT/lib" \
  -o "$OUTPUT_BIN" "$OBJDIR"/*.o "$RUST_LIB" \
  -lc++ -lc++abi -lunwind -lfdio -lzircon -lpthread 2>&1

echo "  Linked: $OUTPUT_BIN"
echo "  $(file "$OUTPUT_BIN")"

# ---------------------------------------------------------------------------
# Step 4: Build ZBI
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 4: Building ZBI ==="

CMDLINE_FILE=$(mktemp)
echo 'zircon.autorun.boot=/boot/bin/rfkill-qemu-demo+/boot/data/modules/rfkill.ko' > "$CMDLINE_FILE"

$ZBI_TOOL -o "$OUTPUT_ZBI" \
  "$ORIG_ZBI" \
  --entry bin/rfkill-qemu-demo="$OUTPUT_BIN" \
  --entry data/modules/rfkill.ko="$RFKILL_KO" \
  -T CMDLINE "$CMDLINE_FILE" 2>&1

rm -f "$CMDLINE_FILE"

echo "  Built: $OUTPUT_ZBI ($(du -h "$OUTPUT_ZBI" | cut -f1))"

# ---------------------------------------------------------------------------
# Step 5: Boot QEMU
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 5: Booting Fuchsia QEMU ($ARCH) ==="
echo "  Timeout: 90 seconds"
echo ""

QEMU_CMD=(
  timeout 90 "$QEMU"
  $QEMU_MACHINE
  -m 4096
  -smp 2
  -nographic
  -no-reboot
  -serial stdio
  -monitor none
  -net none
)

if [ "$NEEDS_KERNEL" = true ]; then
  QEMU_CMD+=($QEMU_KERNEL_FLAG "$KERNEL_PATH" -initrd "$OUTPUT_ZBI")
else
  QEMU_CMD+=($QEMU_KERNEL_FLAG "$OUTPUT_ZBI")
fi

"${QEMU_CMD[@]}" 2>&1 | tee "$QEMU_LOG" || true

# ---------------------------------------------------------------------------
# Step 6: Verify results
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 6: Verifying results ==="
echo ""

if grep -q "RFkill QEMU Demo Results" "$QEMU_LOG"; then
  echo "--- Test output ($ARCH) ---"
  grep -E "rfkill-qemu|rfkill|PASS|FAIL|Results|Total|radios|BLOCK|UNBLOCK|LIST|alloc|register|loaded" "$QEMU_LOG" \
    | sed 's/.*> //' | grep -v "^$"
  echo "---"
  echo ""

  # Count PASS/FAIL — match only test result lines (leading spaces + PASS/FAIL:).
  PASS_COUNT=$(grep -c "^.*>  *PASS:" "$QEMU_LOG" || true)
  FAIL_COUNT=$(grep -c "^.*>  *FAIL: [A-Z]" "$QEMU_LOG" || true)

  echo "  Results: $PASS_COUNT PASS, $FAIL_COUNT FAIL"
  echo ""

  if [ "$FAIL_COUNT" -eq 0 ] && [ "$PASS_COUNT" -ge 18 ]; then
    echo "=== ALL TESTS PASSED ($ARCH) ==="
    exit 0
  else
    echo "=== TESTS FAILED ($ARCH) ==="
    exit 1
  fi
else
  echo "ERROR: RFkill QEMU Demo output not found in QEMU log."
  echo "Check $QEMU_LOG for details."
  echo ""
  echo "Last 30 lines of QEMU output:"
  tail -30 "$QEMU_LOG"
  exit 1
fi
