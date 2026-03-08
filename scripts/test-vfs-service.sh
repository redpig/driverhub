#!/bin/bash
# Test the DriverHub unified VFS services (DevFs, SysFs, ProcFs) + devctl.
#
# This test:
#   1. Builds a test harness that registers devices in all three VFS trees
#   2. Builds the devctl client tool
#   3. Exercises LS, CAT, TREE, STAT, OPEN, CLOSE commands
#
# Usage: scripts/test-vfs-service.sh

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

SOCK="/tmp/driverhub-vfs-test-$$.sock"
SERVER_BIN="/tmp/driverhub-vfs-test-server"
DEVCTL_BIN="/tmp/devctl-test"
PASS=0
FAIL=0
CLEANUP_PIDS=()
SERVER_LOG="/tmp/driverhub-vfs-test-$$.log"

cleanup() {
  for pid in "${CLEANUP_PIDS[@]}"; do
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  done
  rm -f "$SERVER_BIN" "$DEVCTL_BIN" "$SOCK" "$SERVER_LOG" /tmp/vfs_test_main.cc
}
trap cleanup EXIT

echo "=== DriverHub VFS Service Test ==="
echo ""

# --- Step 1: Build test server ---
echo "--- Building VFS service test binary ---"

cat > /tmp/vfs_test_main.cc << 'TESTEOF'
#include <cstdio>
#include <cstring>

#include "src/shim/subsystem/fs.h"
#include "src/shim/subsystem/vfs_service.h"

// Fake file_operations to test DevFs dispatch.
static ssize_t fake_read(struct file* f, char* buf, size_t count, loff_t* pos) {
  const char* data = "hello from /dev/testdev\n";
  size_t len = strlen(data);
  if ((size_t)*pos >= len) return 0;
  size_t to_copy = len - *pos;
  if (to_copy > count) to_copy = count;
  memcpy(buf, data + *pos, to_copy);
  *pos += to_copy;
  return (ssize_t)to_copy;
}

static int fake_open(struct inode* ino, struct file* f) {
  fprintf(stderr, "test: /dev/testdev opened\n");
  return 0;
}

static int fake_release(struct inode* ino, struct file* f) {
  fprintf(stderr, "test: /dev/testdev closed\n");
  return 0;
}

static struct file_operations test_fops = {
  .owner = nullptr,
  .llseek = nullptr,
  .read = fake_read,
  .write = nullptr,
  .unlocked_ioctl = nullptr,
  .compat_ioctl = nullptr,
  .mmap = nullptr,
  .open = fake_open,
  .release = fake_release,
  .fasync = nullptr,
  .poll = nullptr,
};

// Fake show callback for a /proc entry.
static int proc_test_show(struct seq_file* m, void* v) {
  seq_printf(m, "DriverHub VFS test proc entry\nversion: 1.0\n");
  return 0;
}

// Fake sysfs show/store.
static ssize_t sysfs_test_show(struct device* dev,
                                struct device_attribute* attr,
                                char* buf) {
  return snprintf(buf, 4096, "sysfs_test_value\n");
}

static struct device_attribute test_dev_attr = {
  .attr = { .name = "test_attr", .mode = 0644 },
  .show = sysfs_test_show,
  .store = nullptr,
};

int main(int argc, char** argv) {
  const char* sock_path = "/tmp/driverhub-vfs.sock";
  if (argc > 1) sock_path = argv[1];

  // Register a misc device (goes into DevFs automatically via fs.cc hook).
  struct miscdevice test_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "testdev",
    .fops = &test_fops,
    .this_device = nullptr,
    .parent = nullptr,
    .mode = 0666,
  };
  misc_register(&test_misc);

  // Register a proc entry (goes into ProcFs automatically).
  proc_create_single_data("driverhub_test", 0444, nullptr,
                          proc_test_show, nullptr);

  // Register a sysfs device attribute (goes into SysFs automatically).
  struct device test_device;
  memset(&test_device, 0, sizeof(test_device));
  test_device.init_name = "test_device";
  device_create_file(&test_device, &test_dev_attr);

  // Start the VFS server.
  driverhub::StartVfsServer(sock_path);
  fprintf(stderr, "VFS server running on %s\n", sock_path);
  fprintf(stderr, "READY\n");
  fflush(stderr);
  getchar();

  driverhub::StopVfsServer();
  misc_deregister(&test_misc);
  return 0;
}
TESTEOF

CXX="${CXX:-clang++}"
CXXFLAGS="-std=c++17 -Wall -g -I. -pthread"

$CXX $CXXFLAGS -o "$SERVER_BIN" \
  /tmp/vfs_test_main.cc \
  src/shim/subsystem/fs.cc \
  src/shim/subsystem/vfs_service.cc \
  src/shim/subsystem/vfs_server.cc \
  -lpthread 2>&1
echo "  Built: $SERVER_BIN"

# --- Step 2: Build devctl ---
echo "--- Building devctl ---"
$CXX $CXXFLAGS -o "$DEVCTL_BIN" tools/devctl.cc -lpthread 2>&1
echo "  Built: $DEVCTL_BIN"

# --- Step 3: Start the VFS server ---
echo ""
echo "--- Starting VFS service ---"
sleep 300 | "$SERVER_BIN" "$SOCK" 2>"$SERVER_LOG" &
SERVER_PID=$!
CLEANUP_PIDS+=("$SERVER_PID")

# Wait for socket.
for i in $(seq 1 50); do
  if [ -S "$SOCK" ]; then break; fi
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "FAIL: server process died"
    cat "$SERVER_LOG" 2>/dev/null
    exit 1
  fi
  sleep 0.1
done

if [ ! -S "$SOCK" ]; then
  echo "FAIL: server socket not created at $SOCK"
  cat "$SERVER_LOG" 2>/dev/null
  exit 1
fi
echo "  Server running (PID $SERVER_PID, socket $SOCK)"

# --- Step 4: Run tests ---
echo ""
echo "--- Testing devctl ---"

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

# -- DevFs tests --
echo ""
echo "  [DevFs]"
run_test "ls /dev shows testdev" "testdev" "$DEVCTL_BIN" -s "$SOCK" ls /dev
run_test "stat /dev/testdev" "misc" "$DEVCTL_BIN" -s "$SOCK" stat /dev/testdev
run_test "open /dev/testdev" "OK fd=" "$DEVCTL_BIN" -s "$SOCK" open /dev/testdev

# Extract fd from open
FD=$("$DEVCTL_BIN" -s "$SOCK" open /dev/testdev 2>/dev/null | grep -oP 'fd=\K\d+' || echo "0")
if [ "$FD" != "0" ]; then
  run_test "read from open fd" "hello from" "$DEVCTL_BIN" -s "$SOCK" read "$FD" 256
  run_test "close fd" "OK closed" "$DEVCTL_BIN" -s "$SOCK" close "$FD"
else
  echo "  SKIP: could not extract fd from open"
  FAIL=$((FAIL + 1))
fi

# -- ProcFs tests --
echo ""
echo "  [ProcFs]"
run_test "ls /proc shows driverhub_test" "driverhub_test" "$DEVCTL_BIN" -s "$SOCK" ls /proc
run_test "cat /proc/driverhub_test" "version: 1.0" "$DEVCTL_BIN" -s "$SOCK" cat /proc/driverhub_test

# -- SysFs tests --
echo ""
echo "  [SysFs]"
run_test "ls /sys shows devices" "devices" "$DEVCTL_BIN" -s "$SOCK" ls /sys
run_test "cat /sys/devices/test_device/test_attr" "sysfs_test_value" \
  "$DEVCTL_BIN" -s "$SOCK" cat /sys/devices/test_device/test_attr

# -- Tree --
echo ""
echo "  [Tree]"
run_test "tree shows all three roots" "dev" "$DEVCTL_BIN" -s "$SOCK" tree

# -- General --
echo ""
echo "  [General]"
run_test "ls / shows all roots" "devfs" "$DEVCTL_BIN" -s "$SOCK" ls /
run_test "help output" "Usage" "$DEVCTL_BIN" -h
run_test "stat nonexistent" "ERR" "$DEVCTL_BIN" -s "$SOCK" stat /dev/nonexistent

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
  cat "$SERVER_LOG" 2>/dev/null
  exit 1
fi
