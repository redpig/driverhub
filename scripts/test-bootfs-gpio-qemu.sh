#!/bin/bash
# scripts/test-bootfs-gpio-qemu.sh
#
# End-to-end test: cross-compile the DriverHub GPIO bootfs demo for
# Fuchsia x86_64, build a ZBI, boot in QEMU, and verify all tests pass.
#
# Prerequisites:
#   - Fuchsia IDK extracted at third_party/fuchsia-idk/
#   - Fuchsia Clang at third_party/clang/
#   - Fuchsia product bundle at /tmp/fuchsia-pb/ (see docs/fuchsia-env-setup.md)
#   - Host build completed (make all) so .ko modules are built
#
# Usage:
#   ./scripts/test-bootfs-gpio-qemu.sh
#
# The script exits 0 if all 8 GPIO tests pass in the QEMU serial output.

set -euo pipefail

DRIVERHUB="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DRIVERHUB"

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
CXX=third_party/clang/bin/clang++
CC=third_party/clang/bin/clang
IDK=third_party/fuchsia-idk
SYSROOT=$IDK/arch/x64/sysroot
CLANG_RTLIB=third_party/clang/lib/x86_64-unknown-fuchsia
IDK_LIBS=$IDK/arch/x64/lib
ZBI_TOOL=$IDK/tools/x64/zbi
QEMU=$IDK/tools/x64/qemu_internal/bin/qemu-system-x86_64

ORIG_ZBI=/tmp/fuchsia-pb/system_a/fuchsia-orig.zbi
KERNEL=/tmp/fuchsia-pb/system_a/linux-x86-boot-shim.bin

OBJDIR=/tmp/bootfs-gpio-obj
OUTPUT_ZBI=/tmp/fuchsia-bootfs-gpio.zbi
OUTPUT_BIN=/tmp/bootfs-gpio-demo
OUTPUT_KO=/tmp/gpio_controller_module.ko
QEMU_LOG=/tmp/qemu-gpio-output.log

# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------
echo "=== Preflight checks ==="

for f in "$CXX" "$CC" "$ZBI_TOOL" "$QEMU" "$ORIG_ZBI" "$KERNEL"; do
  if [ ! -f "$f" ]; then
    echo "ERROR: missing $f"
    echo "See docs/fuchsia-env-setup.md for setup instructions."
    exit 1
  fi
done

echo "  Toolchain:       $($CXX --version 2>&1 | head -1)"
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

CXXFLAGS="--target=x86_64-unknown-fuchsia --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 -std=c++17 -Wall -g \
  -I. -Isrc/shim/include -fno-pie -fPIC -D__Fuchsia__ $IDK_INCS"

echo "  Found $(echo $IDK_INCS | wc -w) IDK package include dirs"

# ---------------------------------------------------------------------------
# Step 2: Cross-compile all sources
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 2: Cross-compiling for x86_64-unknown-fuchsia ==="

mkdir -p "$OBJDIR"
rm -f "$OBJDIR"/*.o

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
)

COMPILED=0
for src in "${SOURCES[@]}"; do
  obj=$OBJDIR/$(basename "${src%.cc}.o")
  $CXX $CXXFLAGS -c -o "$obj" "$src" 2>&1
  COMPILED=$((COMPILED + 1))
done

echo "  Compiled $COMPILED core sources"

# Compile the Zircon zx library from IDK sources.
ZX_FLAGS="--target=x86_64-unknown-fuchsia --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 -std=c++17 -g -fPIC $IDK_INCS"

ZX_COUNT=0
for src in "$IDK/pkg/zx"/*.cc; do
  obj=$OBJDIR/zx_$(basename "${src%.cc}.o")
  $CXX $ZX_FLAGS -c -o "$obj" "$src" 2>&1
  ZX_COUNT=$((ZX_COUNT + 1))
done

echo "  Compiled $ZX_COUNT zx library sources"

# Compile the demo binary.
$CXX $CXXFLAGS -c -o "$OBJDIR/bootfs_gpio_demo.o" src/fuchsia/bootfs_gpio_demo.cc 2>&1
echo "  Compiled bootfs_gpio_demo.cc"

# ---------------------------------------------------------------------------
# Step 3: Link
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 3: Linking ==="

$CXX --target=x86_64-unknown-fuchsia --sysroot="$SYSROOT" -ffuchsia-api-level=30 \
  -L"$CLANG_RTLIB" \
  -L"$IDK_LIBS" \
  -L"$SYSROOT/lib" \
  -o "$OUTPUT_BIN" "$OBJDIR"/*.o \
  -lc++ -lc++abi -lunwind -lfdio -lzircon -lpthread 2>&1

echo "  Linked: $OUTPUT_BIN"
echo "  $(file "$OUTPUT_BIN")"

# ---------------------------------------------------------------------------
# Step 4: Cross-compile the .ko module
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 4: Cross-compiling GPIO controller .ko ==="

$CC --target=x86_64-unknown-fuchsia -c -o "$OUTPUT_KO" \
  tests/gpio_controller_module.c -Isrc/shim/include \
  -fno-stack-protector -fno-pie 2>&1

echo "  Built: $OUTPUT_KO"
echo "  $(file "$OUTPUT_KO")"

# ---------------------------------------------------------------------------
# Step 5: Build ZBI
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 5: Building ZBI ==="

CMDLINE_FILE=$(mktemp)
echo 'zircon.autorun.boot=/boot/bin/bootfs-gpio-demo+/boot/data/modules/gpio_controller.ko' > "$CMDLINE_FILE"

$ZBI_TOOL -o "$OUTPUT_ZBI" \
  "$ORIG_ZBI" \
  --entry bin/bootfs-gpio-demo="$OUTPUT_BIN" \
  --entry data/modules/gpio_controller.ko="$OUTPUT_KO" \
  -T CMDLINE "$CMDLINE_FILE" 2>&1

rm -f "$CMDLINE_FILE"

echo "  Built: $OUTPUT_ZBI ($(du -h "$OUTPUT_ZBI" | cut -f1))"

# ---------------------------------------------------------------------------
# Step 6: Boot QEMU
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 6: Booting Fuchsia QEMU ==="
echo "  Timeout: 90 seconds"
echo ""

timeout 90 "$QEMU" \
  -kernel "$KERNEL" \
  -initrd "$OUTPUT_ZBI" \
  -m 4096 \
  -smp 2 \
  -nographic \
  -no-reboot \
  -machine q35 \
  -cpu Haswell,+smap,-check,-fsgsbase \
  -serial stdio \
  -monitor none \
  -net none \
  2>&1 | tee "$QEMU_LOG" || true

# ---------------------------------------------------------------------------
# Step 7: Verify results
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 7: Verifying results ==="
echo ""

# Extract the results section from QEMU output.
if grep -q "Bootfs GPIO Provider Results" "$QEMU_LOG"; then
  echo "--- Test output ---"
  grep -E "bootfs-gpio|driverhub-gpio|GPIO|Chip|Pin|Composite|Results|PASS|FAIL|Module loading" "$QEMU_LOG" \
    | sed 's/.*> //' | grep -v "^$"
  echo "---"
  echo ""

  # Count PASS/FAIL.
  PASS_COUNT=$(grep -c "PASS" "$QEMU_LOG" || true)
  FAIL_COUNT=$(grep -c "FAIL" "$QEMU_LOG" || true)

  echo "  Results: $PASS_COUNT PASS, $FAIL_COUNT FAIL"
  echo ""

  if [ "$FAIL_COUNT" -eq 0 ] && [ "$PASS_COUNT" -ge 8 ]; then
    echo "=== ALL TESTS PASSED ==="
    exit 0
  else
    echo "=== TESTS FAILED ==="
    exit 1
  fi
else
  echo "ERROR: DriverHub GPIO demo output not found in QEMU log."
  echo "Check $QEMU_LOG for details."
  echo ""
  echo "Last 20 lines of QEMU output:"
  tail -20 "$QEMU_LOG"
  exit 1
fi
