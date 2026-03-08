#!/bin/bash
# scripts/test-gki-module-qemu.sh
#
# Attempts to load a real GKI aarch64 .ko module through DriverHub in QEMU.
#
# This test exercises the full end-to-end path:
#   1. Cross-compile DriverHub for the target architecture
#   2. Package the real .ko module into a ZBI
#   3. Boot in QEMU
#   4. DriverHub loads the .ko via elfldltl
#   5. Report which symbols resolved and which are missing
#
# The module will likely fail to fully initialize (missing KMI symbols),
# but the test validates:
#   - ELF parsing of a real kernel module
#   - ARM64 relocation processing
#   - Symbol resolution against the KMI shim library
#   - Error reporting for unresolved symbols
#
# Usage:
#   ./scripts/test-gki-module-qemu.sh arm64 test_modules/time_test.ko
#   ./scripts/test-gki-module-qemu.sh x64 test_modules/rfkill.ko

set -euo pipefail

DRIVERHUB="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DRIVERHUB"

if [ $# -lt 2 ]; then
  echo "Usage: $0 <arch> <module.ko>"
  echo ""
  echo "  arch:      x64 or arm64"
  echo "  module.ko: path to a real GKI .ko module"
  echo ""
  echo "Example:"
  echo "  $0 arm64 test_modules/time_test.ko"
  exit 1
fi

ARCH="$1"
KO_PATH="$2"
KO_NAME=$(basename "$KO_PATH")

if [ ! -f "$KO_PATH" ]; then
  echo "ERROR: $KO_PATH not found"
  echo "Run ./scripts/fetch-gki-modules.sh to download test modules."
  exit 1
fi

echo "=== DriverHub Real GKI Module QEMU Test ==="
echo "  Architecture: $ARCH"
echo "  Module:       $KO_PATH"
echo "  Module info:  $(file -b "$KO_PATH" | head -c 80)"
echo ""

# Architecture configuration
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
    echo "ERROR: unknown architecture $ARCH"
    exit 1
    ;;
esac

# Configuration
CXX=third_party/clang/bin/clang++
CC=third_party/clang/bin/clang
IDK=third_party/fuchsia-idk
SYSROOT=$IDK/arch/$IDK_ARCH/sysroot
CLANG_RTLIB=third_party/clang/lib/$CLANG_ARCH
IDK_LIBS=$IDK/arch/$IDK_ARCH/lib
ZBI_TOOL=$IDK/tools/x64/zbi
QEMU=$IDK/tools/x64/qemu_internal/bin/$QEMU_BIN
ORIG_ZBI=$PB_DIR/system_a/fuchsia-orig.zbi

OBJDIR=/tmp/gki-module-obj-$ARCH
OUTPUT_ZBI=/tmp/fuchsia-gki-module-$ARCH.zbi
OUTPUT_BIN=/tmp/driverhub-gki-test-$ARCH
QEMU_LOG=/tmp/qemu-gki-module-$ARCH.log

# Preflight
echo "=== Preflight checks ==="
PREFLIGHT_FILES=("$CXX" "$CC" "$ZBI_TOOL" "$QEMU" "$ORIG_ZBI")
if [ "$NEEDS_KERNEL" = true ]; then
  KERNEL_PATH="$PB_DIR/system_a/$BOOT_KERNEL"
  PREFLIGHT_FILES+=("$KERNEL_PATH")
fi

for f in "${PREFLIGHT_FILES[@]}"; do
  if [ ! -f "$f" ]; then
    echo "ERROR: missing $f"
    exit 1
  fi
done
echo "  All prerequisites found"

# IDK includes
IDK_INCS=""
for d in $(find "$IDK/pkg" -name include -type d 2>/dev/null); do
  IDK_INCS="$IDK_INCS -I$d"
done

CXXFLAGS="--target=$TARGET_TRIPLE --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 -std=c++17 -Wall -g \
  -I. -Isrc/shim/include -fno-pie -fPIC -D__Fuchsia__ $IDK_INCS"

# Cross-compile
echo ""
echo "=== Cross-compiling DriverHub for $TARGET_TRIPLE ==="

mkdir -p "$OBJDIR"
rm -f "$OBJDIR"/*.o

SOURCES=(
  src/main.cc
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
  src/shim/kernel/rfkill_stubs.cc
)

COMPILED=0
for src in "${SOURCES[@]}"; do
  obj=$OBJDIR/$(basename "${src%.cc}.o")
  $CXX $CXXFLAGS -c -o "$obj" "$src" 2>&1
  COMPILED=$((COMPILED + 1))
done
echo "  Compiled $COMPILED sources"

# zx library
ZX_FLAGS="--target=$TARGET_TRIPLE --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 -std=c++17 -g -fPIC $IDK_INCS"
for src in "$IDK/pkg/zx"/*.cc; do
  obj=$OBJDIR/zx_$(basename "${src%.cc}.o")
  $CXX $ZX_FLAGS -c -o "$obj" "$src" 2>&1
done

# Link
echo ""
echo "=== Linking ==="
$CXX --target="$TARGET_TRIPLE" --sysroot="$SYSROOT" -ffuchsia-api-level=30 \
  -L"$CLANG_RTLIB" -L"$IDK_LIBS" -L"$SYSROOT/lib" \
  -o "$OUTPUT_BIN" "$OBJDIR"/*.o \
  -lc++ -lc++abi -lunwind -lfdio -lzircon -lpthread 2>&1
echo "  Linked: $OUTPUT_BIN"

# Build ZBI
echo ""
echo "=== Building ZBI ==="
CMDLINE_FILE=$(mktemp)
echo "zircon.autorun.boot=/boot/bin/driverhub+/boot/data/modules/$KO_NAME" > "$CMDLINE_FILE"

$ZBI_TOOL -o "$OUTPUT_ZBI" \
  "$ORIG_ZBI" \
  --entry bin/driverhub="$OUTPUT_BIN" \
  --entry "data/modules/$KO_NAME=$KO_PATH" \
  -T CMDLINE "$CMDLINE_FILE" 2>&1
rm -f "$CMDLINE_FILE"
echo "  Built: $OUTPUT_ZBI"

# Boot QEMU
echo ""
echo "=== Booting Fuchsia QEMU ($ARCH) — loading $KO_NAME ==="
echo "  Timeout: 90 seconds"
echo ""

QEMU_CMD=(
  timeout 90 "$QEMU"
  $QEMU_MACHINE
  -m 4096 -smp 2 -nographic -no-reboot
  -serial stdio -monitor none -net none
)

if [ "$NEEDS_KERNEL" = true ]; then
  QEMU_CMD+=($QEMU_KERNEL_FLAG "$KERNEL_PATH" -initrd "$OUTPUT_ZBI")
else
  QEMU_CMD+=($QEMU_KERNEL_FLAG "$OUTPUT_ZBI")
fi

"${QEMU_CMD[@]}" 2>&1 | tee "$QEMU_LOG" || true

# Analyze results
echo ""
echo "=== Results for $KO_NAME ($ARCH) ==="
echo ""

if grep -q "driverhub:" "$QEMU_LOG"; then
  echo "--- DriverHub output ---"
  grep "driverhub:" "$QEMU_LOG" | sed 's/.*> //'
  echo "---"
  echo ""

  if grep -q "init_module succeeded" "$QEMU_LOG"; then
    echo "RESULT: $KO_NAME loaded and initialized SUCCESSFULLY"
  elif grep -q "unresolved symbol" "$QEMU_LOG"; then
    UNRESOLVED=$(grep -c "unresolved symbol" "$QEMU_LOG" || true)
    echo "RESULT: $KO_NAME parsed but has $UNRESOLVED unresolved symbol(s)"
    echo ""
    echo "Missing symbols:"
    grep "unresolved symbol" "$QEMU_LOG" | sed 's/.*unresolved symbol: /  /' | sort -u
  elif grep -q "failed to load" "$QEMU_LOG"; then
    echo "RESULT: $KO_NAME failed to load (ELF/relocation error)"
  else
    echo "RESULT: Unknown outcome — check log"
  fi
else
  echo "DriverHub output not found in QEMU log."
  echo "Check $QEMU_LOG for details."
fi

echo ""
echo "Full log: $QEMU_LOG"
