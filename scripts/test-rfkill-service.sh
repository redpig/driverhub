#!/bin/bash
# Test the DriverHub rfkill service + rfkillctl client tool.
#
# This test:
#   1. Builds DriverHub natively (host) with rfkill service
#   2. Builds rfkillctl natively
#   3. Starts DriverHub loading a dummy rfkill scenario
#   4. Uses rfkillctl to list, block, and unblock radios
#
# Usage: scripts/test-rfkill-service.sh

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

SOCK="/tmp/rfkill-test-$$.sock"
DRIVERHUB_BIN="/tmp/driverhub-rfkill-test"
RFKILLCTL_BIN="/tmp/rfkillctl-test"
PASS=0
FAIL=0
CLEANUP_PIDS=()

cleanup() {
  for pid in "${CLEANUP_PIDS[@]}"; do
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  done
  rm -f "$DRIVERHUB_BIN" "$RFKILLCTL_BIN" "$SOCK" "${SERVER_LOG:-}" /tmp/rfkill_test_main.cc
}
trap cleanup EXIT

echo "=== DriverHub rfkill Service Test ==="
echo ""

# --- Step 1: Build a minimal native test program ---
echo "--- Building rfkill service test binary ---"

# Build a small test that exercises the rfkill service directly
# (no ELF loading, just the service layer + server + client).
cat > /tmp/rfkill_test_main.cc << 'TESTEOF'
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>

#include "src/shim/subsystem/rfkill.h"
#include "src/shim/subsystem/rfkill_server.h"

int main(int argc, char** argv) {
  const char* sock_path = "/tmp/rfkill.sock";
  if (argc > 1) sock_path = argv[1];

  auto& svc = driverhub::RfkillService::Instance();

  // Register some test radios to simulate what WiFi/BT drivers would do.
  uint32_t wifi_idx = svc.RegisterRadio(driverhub::RfkillType::kWlan, "phy0");
  uint32_t bt_idx = svc.RegisterRadio(driverhub::RfkillType::kBluetooth, "hci0");
  uint32_t nfc_idx = svc.RegisterRadio(driverhub::RfkillType::kNfc, "nfc0");

  fprintf(stderr, "Registered radios: wifi=%u bt=%u nfc=%u\n",
          wifi_idx, bt_idx, nfc_idx);

  // Start server.
  driverhub::StartRfkillServer(sock_path);
  fprintf(stderr, "rfkill server running on %s\n", sock_path);

  // Wait for Enter to exit.
  fprintf(stderr, "READY\n");
  fflush(stderr);
  getchar();

  driverhub::StopRfkillServer();
  return 0;
}
TESTEOF

CXX="${CXX:-clang++}"
CXXFLAGS="-std=c++17 -Wall -g -I. -pthread"

$CXX $CXXFLAGS -o "$DRIVERHUB_BIN" \
  /tmp/rfkill_test_main.cc \
  src/shim/subsystem/rfkill.cc \
  src/shim/subsystem/rfkill_server.cc \
  -lpthread 2>&1
echo "  Built: $DRIVERHUB_BIN"

# --- Step 2: Build rfkillctl ---
echo "--- Building rfkillctl ---"
$CXX $CXXFLAGS -o "$RFKILLCTL_BIN" tools/rfkillctl.cc -lpthread 2>&1
echo "  Built: $RFKILLCTL_BIN"

# --- Step 3: Start the server ---
echo ""
echo "--- Starting rfkill service ---"
# Keep the server alive by feeding it from sleep (blocks stdin for 300s).
SERVER_LOG="/tmp/rfkill-test-log-$$"
sleep 300 | "$DRIVERHUB_BIN" "$SOCK" 2>"$SERVER_LOG" &
SERVER_PID=$!
CLEANUP_PIDS+=("$SERVER_PID")

# Wait for the server to be ready (socket file appears).
for i in $(seq 1 50); do
  if [ -S "$SOCK" ]; then
    break
  fi
  # Check the server is still alive.
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "FAIL: server process died"
    cat "$SERVER_LOG"
    exit 1
  fi
  sleep 0.1
done

if [ ! -S "$SOCK" ]; then
  echo "FAIL: server socket not created at $SOCK"
  cat "$SERVER_LOG"
  exit 1
fi
echo "  Server running (PID $SERVER_PID, socket $SOCK)"

# --- Step 4: Test rfkillctl commands ---
echo ""
echo "--- Testing rfkillctl ---"

run_test() {
  local desc="$1"
  local expected="$2"
  shift 2
  local output
  output=$("$@" 2>&1) || true
  if echo "$output" | grep -q "$expected"; then
    echo "  PASS: $desc"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: $desc"
    echo "    expected: $expected"
    echo "    got: $output"
    FAIL=$((FAIL + 1))
  fi
}

# Test: list radios
run_test "list radios" "OK 3 radios" "$RFKILLCTL_BIN" -s "$SOCK" list

# Test: list shows wifi
run_test "list shows phy0" "phy0" "$RFKILLCTL_BIN" -s "$SOCK" list

# Test: list shows bluetooth
run_test "list shows hci0" "hci0" "$RFKILLCTL_BIN" -s "$SOCK" list

# Test: list shows nfc
run_test "list shows nfc0" "nfc0" "$RFKILLCTL_BIN" -s "$SOCK" list

# Test: block radio 0
run_test "block radio 0" "OK blocked 0" "$RFKILLCTL_BIN" -s "$SOCK" block 0

# Verify block shows in list
run_test "radio 0 shows blocked" "soft=blocked" "$RFKILLCTL_BIN" -s "$SOCK" list

# Test: unblock radio 0
run_test "unblock radio 0" "OK unblocked 0" "$RFKILLCTL_BIN" -s "$SOCK" unblock 0

# Verify unblock shows in list
run_test "radio 0 shows unblocked" "soft=unblocked" "$RFKILLCTL_BIN" -s "$SOCK" list

# Test: block all (airplane mode)
run_test "block all radios" "OK blocked all" "$RFKILLCTL_BIN" -s "$SOCK" block all

# Test: unblock all wlan
run_test "unblock all wlan" "OK unblocked all wlan" "$RFKILLCTL_BIN" -s "$SOCK" unblock all wlan

# Test: block all bluetooth
run_test "block all bluetooth" "OK blocked all bluetooth" "$RFKILLCTL_BIN" -s "$SOCK" block all bluetooth

# Test: invalid index
run_test "block invalid index" "ERR" "$RFKILLCTL_BIN" -s "$SOCK" block 99

# Test: help
run_test "help output" "Usage" "$RFKILLCTL_BIN" -h

echo ""
echo "=== Results ==="
echo "  PASS: $PASS"
echo "  FAIL: $FAIL"
echo ""

if [ "$FAIL" -eq 0 ]; then
  echo "All tests passed!"
  exit 0
else
  echo "Some tests failed."
  exit 1
fi
