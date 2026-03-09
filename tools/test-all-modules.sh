#!/bin/bash
# Copyright 2024 The DriverHub Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Bulk module loading test: attempts to load every .ko from a factory image
# extraction and reports which symbols are missing.
#
# Usage:
#   ./tools/test-all-modules.sh <modules-dir> [driverhub-binary]
#
# Example:
#   ./tools/extract-factory/target/release/extract-factory \
#       --zip factory.zip --output /tmp/pixel --list-symbols
#   ./tools/test-all-modules.sh /tmp/pixel/modules ./build/driverhub

set -euo pipefail

MODULES_DIR="${1:?Usage: $0 <modules-dir> [driverhub-binary]}"
DRIVERHUB="${2:-./build/driverhub}"

if [ ! -d "$MODULES_DIR" ]; then
    echo "Error: modules directory not found: $MODULES_DIR" >&2
    exit 1
fi

if [ ! -x "$DRIVERHUB" ]; then
    echo "Error: driverhub binary not found or not executable: $DRIVERHUB" >&2
    echo "Build with: make" >&2
    exit 1
fi

# Count modules.
MODULES=("$MODULES_DIR"/*.ko)
TOTAL=${#MODULES[@]}

if [ "$TOTAL" -eq 0 ]; then
    echo "No .ko modules found in $MODULES_DIR" >&2
    exit 1
fi

echo "=== DriverHub Bulk Module Loading Test ==="
echo "Modules directory: $MODULES_DIR"
echo "DriverHub binary:  $DRIVERHUB"
echo "Total modules:     $TOTAL"
echo ""

# Results tracking.
PASS=0
FAIL=0
CRASH=0
SKIP=0
RESULTS_FILE=$(mktemp)
MISSING_SYMBOLS_FILE=$(mktemp)

trap 'rm -f "$RESULTS_FILE" "$MISSING_SYMBOLS_FILE"' EXIT

for ko in "${MODULES[@]}"; do
    MODULE_NAME=$(basename "$ko")

    # Run driverhub with a timeout, feeding Enter to trigger unload.
    OUTPUT=$(echo "" | timeout 10 "$DRIVERHUB" "$ko" 2>&1 || true)

    # Check results.
    if echo "$OUTPUT" | grep -q "module(s) loaded"; then
        # Module loaded (possibly with warnings).
        if echo "$OUTPUT" | grep -q "error:"; then
            STATUS="FAIL"
            FAIL=$((FAIL + 1))
        else
            STATUS="PASS"
            PASS=$((PASS + 1))
        fi
    elif echo "$OUTPUT" | grep -q "FAULT\|SIGSEGV\|SIGBUS\|SIGILL"; then
        STATUS="CRASH"
        CRASH=$((CRASH + 1))
    elif echo "$OUTPUT" | grep -q "unresolved symbol"; then
        STATUS="FAIL"
        FAIL=$((FAIL + 1))
        # Extract unresolved symbols.
        echo "$OUTPUT" | grep "unresolved symbol" | \
            sed 's/.*unresolved symbol: //' >> "$MISSING_SYMBOLS_FILE"
    else
        STATUS="FAIL"
        FAIL=$((FAIL + 1))
    fi

    printf "  %-50s %s\n" "$MODULE_NAME" "$STATUS"
    echo "$MODULE_NAME $STATUS" >> "$RESULTS_FILE"
done

echo ""
echo "=== Results ==="
echo "  PASS:  $PASS / $TOTAL"
echo "  FAIL:  $FAIL / $TOTAL"
echo "  CRASH: $CRASH / $TOTAL"
echo ""

# Report most commonly missing symbols.
if [ -s "$MISSING_SYMBOLS_FILE" ]; then
    echo "=== Most Commonly Missing Symbols ==="
    sort "$MISSING_SYMBOLS_FILE" | uniq -c | sort -rn | head -30
    echo ""

    UNIQUE_MISSING=$(sort -u "$MISSING_SYMBOLS_FILE" | wc -l)
    echo "Total unique missing symbols: $UNIQUE_MISSING"
fi

# Save detailed results.
REPORT="$MODULES_DIR/../module_load_results.txt"
cp "$RESULTS_FILE" "$REPORT"
echo ""
echo "Detailed results saved to: $REPORT"

# Exit with failure if any module failed.
if [ "$FAIL" -gt 0 ] || [ "$CRASH" -gt 0 ]; then
    exit 1
fi
