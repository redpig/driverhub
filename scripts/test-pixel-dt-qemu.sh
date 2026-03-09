#!/bin/bash
# scripts/test-pixel-dt-qemu.sh
#
# Boot DriverHub in QEMU aarch64 with the Pixel 10 device tree and all
# GKI modules from the system_dlkm partition. Shows DT-driven enumeration
# of the full module set.
#
# Usage:
#   ./scripts/test-pixel-dt-qemu.sh [module_dir]
#
# Defaults to /tmp/gki-artifacts/modules/flatten/lib/modules/

set -euo pipefail

DRIVERHUB="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DRIVERHUB"

MODULE_DIR="${1:-/tmp/gki-artifacts/modules/flatten/lib/modules}"
DTB_PATH="tests/fixtures/pixel10-test.dtb"

ARCH=arm64
TARGET_TRIPLE=aarch64-unknown-fuchsia
CLANG="$DRIVERHUB/third_party/clang/bin/clang++"
IDK="$DRIVERHUB/third_party/fuchsia-idk"
SYSROOT="$IDK/arch/arm64/sysroot"
IDK_INCS=""
for d in $(find "$IDK/pkg" -name include -type d 2>/dev/null); do
  IDK_INCS="$IDK_INCS -I$d"
done
IDK_LIBS="$IDK/arch/arm64/lib"
CLANG_RTLIB="$DRIVERHUB/third_party/clang/lib/aarch64-unknown-fuchsia/c++"
QEMU_BIN=qemu-system-aarch64
QEMU="$IDK/tools/x64/qemu_internal/bin/$QEMU_BIN"
QEMU_MACHINE="-machine virt,gic-version=3,virtualization=on -cpu cortex-a53"
ZBI_TOOL="$IDK/tools/x64/zbi"
PB_DIR=/tmp/fuchsia-pb-arm64
KERNEL_PATH="$PB_DIR/system_a/linux-arm64-boot-shim.bin"

OBJDIR=/tmp/pixel-dt-obj-arm64
OUTPUT_BIN=/tmp/driverhub-pixel-dt-arm64
OUTPUT_ZBI=/tmp/fuchsia-pixel-dt-arm64.zbi
QEMU_LOG=/tmp/qemu-pixel-dt-arm64.log

echo "=== DriverHub Pixel 10 DT Enumeration Test ==="
echo "  Architecture: arm64"
echo "  Device tree:  $DTB_PATH"
echo "  Module dir:   $MODULE_DIR"
MODULE_COUNT=$(ls "$MODULE_DIR"/*.ko 2>/dev/null | wc -l)
echo "  Modules:      $MODULE_COUNT"
echo ""

# Preflight
echo "=== Preflight checks ==="
for req in "$CLANG" "$QEMU" "$ZBI_TOOL" "$KERNEL_PATH" "$DTB_PATH"; do
  if [ ! -f "$req" ]; then
    echo "MISSING: $req"
    exit 1
  fi
done
ORIG_ZBI=$(find "$PB_DIR" -name "fuchsia.zbi" 2>/dev/null | head -1)
if [ -z "$ORIG_ZBI" ]; then
  echo "MISSING: Fuchsia ZBI in $PB_DIR"
  exit 1
fi
echo "  All prerequisites found"
echo ""

# Cross-compile
echo "=== Cross-compiling DriverHub for $TARGET_TRIPLE ==="
mkdir -p "$OBJDIR"

CXXFLAGS="--target=$TARGET_TRIPLE --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 -std=c++17 -Wall -g \
  -I. -Isrc/shim/include -fno-pie -fPIC -D__Fuchsia__ $IDK_INCS"

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
  src/shim/subsystem/net.cc
  src/shim/subsystem/vfs_service.cc
  src/shim/kernel/rfkill_stubs.cc
  src/shim/kernel/libc_stubs.cc
  src/shim/kernel/gki_stubs.cc
)

COMPILED=0
for src in "${SOURCES[@]}"; do
  obj=$OBJDIR/$(basename "${src%.cc}.o")
  $CLANG $CXXFLAGS -c -o "$obj" "$src" 2>&1
  COMPILED=$((COMPILED + 1))
done
echo "  Compiled $COMPILED sources"

# zx library
ZX_FLAGS="--target=$TARGET_TRIPLE --sysroot=$SYSROOT \
  -ffuchsia-api-level=30 -std=c++17 -g -fPIC $IDK_INCS"
for src in "$IDK/pkg/zx"/*.cc; do
  obj=$OBJDIR/zx_$(basename "${src%.cc}.o")
  $CLANG $ZX_FLAGS -c -o "$obj" "$src" 2>&1
done

# Link
echo ""
echo "=== Linking ==="
$CLANG --target="$TARGET_TRIPLE" --sysroot="$SYSROOT" -ffuchsia-api-level=30 \
  -L"$CLANG_RTLIB" -L"$IDK_LIBS" -L"$SYSROOT/lib" \
  -o "$OUTPUT_BIN" "$OBJDIR"/*.o \
  -lc++ -lc++abi -lunwind -lfdio -lzircon -lpthread 2>&1
echo "  Linked: $OUTPUT_BIN"

# Build ZBI with ALL modules + DTB
echo ""
echo "=== Building ZBI with $MODULE_COUNT modules + DTB ==="

# Use a manifest file to avoid bootfs directory enumeration (triggers a
# Zircon kernel panic) and to stay within the 128-byte autorun limit.
CMDLINE_FILE=$(mktemp)
echo "zircon.autorun.boot=/boot/bin/driverhub+--manifest+/boot/data/modules.txt" > "$CMDLINE_FILE"

# Build manifest listing all module paths
MANIFEST_FILE=$(mktemp)
for ko in "$MODULE_DIR"/*.ko; do
  echo "/boot/data/modules/$(basename "$ko")" >> "$MANIFEST_FILE"
done

# Build ZBI entries for all modules
ZBI_ENTRIES=()
ZBI_ENTRIES+=(--entry bin/driverhub="$OUTPUT_BIN")
ZBI_ENTRIES+=(--entry "data/dt/pixel10-test.dtb=$DTB_PATH")
ZBI_ENTRIES+=(--entry "data/modules.txt=$MANIFEST_FILE")
for ko in "$MODULE_DIR"/*.ko; do
  ZBI_ENTRIES+=(--entry "data/modules/$(basename "$ko")=$ko")
done

$ZBI_TOOL -o "$OUTPUT_ZBI" \
  "$ORIG_ZBI" \
  "${ZBI_ENTRIES[@]}" \
  -T CMDLINE "$CMDLINE_FILE" 2>&1
rm -f "$CMDLINE_FILE" "$MANIFEST_FILE"
echo "  Built: $OUTPUT_ZBI ($(du -h "$OUTPUT_ZBI" | cut -f1))"

# Boot QEMU
echo ""
echo "=== Booting Fuchsia QEMU (arm64) — loading $MODULE_COUNT modules ==="
echo "  Timeout: 180 seconds"
echo ""

QEMU_CMD=(
  timeout 180 "$QEMU"
  $QEMU_MACHINE
  -m 8192 -smp 2 -nographic -no-reboot
  -serial stdio -monitor none -net none
  -kernel "$KERNEL_PATH" -initrd "$OUTPUT_ZBI"
)

"${QEMU_CMD[@]}" 2>&1 | tee "$QEMU_LOG" || true

# Analyze results
echo ""
echo "=== Results — Pixel 10 DT Enumeration ($MODULE_COUNT modules) ==="
echo ""

if grep -q "driverhub:" "$QEMU_LOG"; then
  # Count results
  LOADED=$(grep -c "init_module succeeded" "$QEMU_LOG" 2>/dev/null || true)
  FAILED=$(grep -c "init_module returned" "$QEMU_LOG" 2>/dev/null || true)
  FAILED=$((FAILED - LOADED))  # subtract successes
  ELF_ERR=$(grep -c "failed to load\|wrong architecture" "$QEMU_LOG" 2>/dev/null || true)
  KMI_REG=$(grep "registered.*KMI" "$QEMU_LOG" | tail -1 | sed 's/.*registered //' | sed 's/ KMI.*//')

  echo "KMI symbols registered: $KMI_REG"
  echo ""
  echo "Module results:"
  echo "  init_module succeeded:  $LOADED"
  echo "  init_module failed:     $FAILED"
  echo "  ELF load errors:        $ELF_ERR"
  echo ""

  echo "Successful modules:"
  grep "init_module succeeded" "$QEMU_LOG" | sed 's/.*> driverhub: /  /' | sed 's/.*driverhub: /  /'

  echo ""
  echo "Failed modules:"
  grep "init_module returned" "$QEMU_LOG" | grep -v "succeeded" | sed 's/.*> driverhub: /  /' | sed 's/.*driverhub: /  /' | head -20
fi

echo ""
echo "Full log: $QEMU_LOG"
