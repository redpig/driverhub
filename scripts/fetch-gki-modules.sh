#!/bin/bash
# scripts/fetch-gki-modules.sh
#
# Downloads real Android GKI arm64 kernel modules from the Android kernel
# prebuilts repository on googlesource.com for testing.
#
# These are production .ko modules from the Android 15 / kernel 6.6 GKI
# build. They exercise DriverHub's ELF loader with real-world relocations
# and symbol dependencies.
#
# Usage:
#   ./scripts/fetch-gki-modules.sh              # Download to test_modules/
#   ./scripts/fetch-gki-modules.sh <output_dir>  # Download to custom dir

set -euo pipefail

DRIVERHUB="$(cd "$(dirname "$0")/.." && pwd)"
OUTDIR="${1:-$DRIVERHUB/test_modules}"
mkdir -p "$OUTDIR"

BASE_URL="https://android.googlesource.com/kernel/prebuilts/6.6/arm64/+/refs/heads/main"

# Modules to download, ordered by complexity (simplest first).
MODULES=(
  "time_test.ko"         # ~15KB, 4 external symbols
  "input_test.ko"        # ~25KB, input subsystem test
  "rfkill.ko"            # ~60KB, real driver with many KMI symbols
  "clk-gate_test.ko"     # ~61KB, clock subsystem test
)

echo "=== Fetching Android GKI arm64 kernel modules ==="
echo "  Source: $BASE_URL"
echo "  Output: $OUTDIR"
echo ""

for mod in "${MODULES[@]}"; do
  echo -n "  Downloading $mod ... "
  if curl -sL "${BASE_URL}/${mod}?format=TEXT" | base64 -d > "$OUTDIR/$mod" 2>/dev/null; then
    size=$(du -h "$OUTDIR/$mod" | cut -f1)
    arch=$(file -b "$OUTDIR/$mod" | head -c 50)
    echo "ok ($size, $arch)"
  else
    echo "FAILED"
    rm -f "$OUTDIR/$mod"
  fi
done

echo ""
echo "=== Downloaded modules ==="
ls -lh "$OUTDIR"/*.ko 2>/dev/null || echo "  (none)"
echo ""
echo "To test with DriverHub QEMU (aarch64):"
echo "  ./scripts/test-gki-module-qemu.sh arm64 $OUTDIR/time_test.ko"
