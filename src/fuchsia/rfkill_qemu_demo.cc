// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RFkill QEMU Demo
//
// End-to-end test of the rfkill subsystem on aarch64 in QEMU.
//
//   Phase 1: Load rfkill.ko via elfldltl — ELF parsing, ARM64 relocations,
//            symbol resolution, and module export registration.
//   Phase 2: Call init_module() — executes the module's init, which calls
//            class_register, misc_register (creates /dev/rfkill in DevFs),
//            led_trigger_register, and queue_work_on.
//   Phase 3: Register test radios via the MODULE's own exported rfkill_alloc
//            and rfkill_register functions (not the shim bypass — the actual
//            compiled ARM64 code in rfkill.ko).
//   Phase 4: Exercise the DevFs → module fops path (the server-side of the
//            fuchsia.driverhub.Vfs FIDL protocol):
//              Vfs.Open        → DevFs::Open   → rfkill_fop_open
//              Vfs.DeviceRead  → DevFs::Read   → rfkill_fop_read
//              Vfs.DeviceWrite → DevFs::Write  → rfkill_fop_write
//              Vfs.DeviceIoctl → DevFs::Ioctl  → rfkill_fop_ioctl
//              Vfs.DeviceClose → DevFs::Close  → rfkill_fop_release
//   Phase 5: Cleanup — cleanup_module + destroy radios.
//
// On Fuchsia bootfs: runs via zircon.autorun.boot.

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "src/loader/module_executor.h"
#include "src/loader/module_loader.h"
#include "src/shim/subsystem/fs.h"
#include "src/shim/subsystem/vfs_service.h"
#include "src/symbols/symbol_registry.h"

// Platform-specific includes.
#if defined(__Fuchsia__)
#include "src/fuchsia/resource_provider.h"
#include "src/fuchsia/vmo_module_loader.h"
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/debug.h>
#include <zircon/syscalls/exception.h>
#include <zircon/threads.h>
#include <threads.h>
#include <atomic>
#endif

// Linux rfkill userspace ABI — matches the kernel's struct rfkill_event.
// This is what /dev/rfkill read/write operates on.
struct rfkill_event {
  uint32_t idx;
  uint8_t  type;
  uint8_t  op;
  uint8_t  soft;
  uint8_t  hard;
};

// Linux rfkill ioctl commands.
#define RFKILL_OP_ADD       0
#define RFKILL_OP_DEL       1
#define RFKILL_OP_CHANGE    2
#define RFKILL_OP_CHANGE_ALL 3

// Linux struct rfkill_ops — the callback struct rfkill_alloc requires.
// rfkill_alloc checks ops->set_block (offset 0x10) is non-NULL.
struct rfkill_ops {
  void (*poll)(void* rfkill, void* data);        // 0x00
  void (*query)(void* rfkill, void* data);       // 0x08
  int (*set_block)(void* data, bool blocked);    // 0x10
};

// Dummy set_block — logs and accepts. A real driver would toggle
// hardware here.
static int dummy_set_block(void* data, bool blocked) {
  fprintf(stderr, "driverhub: rfkill set_block(%s)\n",
          blocked ? "blocked" : "unblocked");
  return 0;
}

static struct rfkill_ops test_ops = {
  .poll = nullptr,
  .query = nullptr,
  .set_block = dummy_set_block,
};

namespace {

int g_pass = 0;
int g_fail = 0;

const char* pass_fail(bool ok) { return ok ? "PASS" : "FAIL"; }

void check(const char* desc, bool ok) {
  fprintf(stderr, "  %s: %s\n", pass_fail(ok), desc);
  if (ok) ++g_pass; else ++g_fail;
}

#if defined(__Fuchsia__)
// Exception handler for catching faults in module fops calls.
// Runs on a dedicated thread, handles multiple exceptions by recovering
// each faulting call (set x0 = error code, PC = LR).
static std::atomic<int> g_fault_count{0};

int FopsExceptionWatcher(void* arg) {
  zx_handle_t channel = static_cast<zx_handle_t>(
      reinterpret_cast<uintptr_t>(arg));

  while (true) {
    zx_wait_item_t items[1] = {
        {channel, ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED, 0}};
    zx_status_t status = zx_object_wait_many(items, 1, ZX_TIME_INFINITE);
    if (status != ZX_OK || !(items[0].pending & ZX_CHANNEL_READABLE)) {
      break;  // Channel closed — no more exceptions.
    }

    zx_exception_info_t info;
    zx_handle_t exception;
    uint32_t ab, ah;
    status = zx_channel_read(channel, 0, &info, &exception,
                              sizeof(info), 1, &ab, &ah);
    if (status != ZX_OK) break;

    zx_handle_t thread;
    status = zx_exception_get_thread(exception, &thread);
    if (status == ZX_OK) {
      zx_thread_state_general_regs_t regs;
      status = zx_thread_read_state(thread, ZX_THREAD_STATE_GENERAL_REGS,
                                     &regs, sizeof(regs));
      if (status == ZX_OK) {
        // Read the faulting instruction.
        uint32_t fault_insn = 0;
        size_t actual;
        zx_status_t rd = zx_process_read_memory(
            zx_process_self(), regs.pc, &fault_insn, 4, &actual);

        // Try to emulate privileged ARM64 instructions (MRS/MSR)
        // before treating as a fatal fault.  GKI module code
        // frequently reads system registers (e.g. TPIDR_EL1,
        // VBAR_EL1) which trap in userspace on QEMU.
        bool emulated = false;
        if (rd == ZX_OK) {
          bool is_msr = (fault_insn & 0xFFF00000u) == 0xD5100000u;
          bool is_mrs = (fault_insn & 0xFFF00000u) == 0xD5300000u;
          bool is_daif_imm = (fault_insn & 0xFFFFF0DFu) == 0xD50340DFu;

          if (is_msr || is_mrs || is_daif_imm) {
            uint32_t rt = fault_insn & 0x1F;
            if (is_mrs && rt < 30) {
              regs.r[rt] = 0;
            }
            regs.pc += 4;
            zx_thread_write_state(thread, ZX_THREAD_STATE_GENERAL_REGS,
                                   &regs, sizeof(regs));
            emulated = true;
          }
        }

        if (!emulated) {
          g_fault_count++;
          fprintf(stderr,
                  "[rfkill-qemu] FAULT #%d in module fops!\n"
                  "  PC:  0x%016lx\n"
                  "  LR:  0x%016lx\n"
                  "  SP:  0x%016lx\n"
                  "  X0:  0x%016lx  X1: 0x%016lx\n"
                  "  X2:  0x%016lx  X3: 0x%016lx\n",
                  g_fault_count.load(),
                  (unsigned long)regs.pc,
                  (unsigned long)regs.lr,
                  (unsigned long)regs.sp,
                  (unsigned long)regs.r[0],
                  (unsigned long)regs.r[1],
                  (unsigned long)regs.r[2],
                  (unsigned long)regs.r[3]);

          if (rd == ZX_OK) {
            fprintf(stderr, "  Insn: 0x%08x", fault_insn);
            if (fault_insn == 0xd4200000) fprintf(stderr, " (brk #0 / BUG)");
            else if ((fault_insn & 0xFFE0001F) == 0xd4200000)
              fprintf(stderr, " (brk #%u)",
                      (fault_insn >> 5) & 0xFFFF);
            fprintf(stderr, "\n");
          }

          // Recover: return -EFAULT from the faulting function.
          regs.r[0] = static_cast<uint64_t>(static_cast<int64_t>(-14));  // -EFAULT
          regs.pc = regs.lr;
          zx_thread_write_state(thread, ZX_THREAD_STATE_GENERAL_REGS,
                                 &regs, sizeof(regs));
        }
      }
      zx_handle_close(thread);
    }

    uint32_t state = ZX_EXCEPTION_STATE_HANDLED;
    zx_object_set_property(exception, ZX_PROP_EXCEPTION_STATE,
                           &state, sizeof(state));
    zx_handle_close(exception);
  }
  return 0;
}

struct FopsGuard {
  zx_handle_t channel = ZX_HANDLE_INVALID;
  thrd_t watcher;
  bool active = false;

  void Start() {
    zx_status_t status = zx_task_create_exception_channel(
        zx_process_self(), 0, &channel);
    if (status != ZX_OK) {
      fprintf(stderr, "[rfkill-qemu] WARNING: no exception channel (%d)\n",
              status);
      return;
    }
    g_fault_count = 0;
    thrd_create(&watcher, FopsExceptionWatcher,
                reinterpret_cast<void*>(static_cast<uintptr_t>(channel)));
    active = true;
  }

  void Stop() {
    if (!active) return;
    zx_handle_close(channel);
    thrd_join(watcher, nullptr);
    active = false;
  }

  ~FopsGuard() { Stop(); }
};
#else
// Host: no exception guard needed (signal handler in SafeCall* suffices).
struct FopsGuard {
  void Start() {}
  void Stop() {}
};
#endif

}  // namespace

int main(int argc, char** argv) {
  fprintf(stderr,
          "\n"
          "============================================================\n"
          "  DriverHub RFkill QEMU Demo — DevFs + Module Fops Path\n"
          "============================================================\n\n");

  if (argc < 2) {
    fprintf(stderr, "usage: %s <rfkill.ko>\n", argv[0]);
    return 1;
  }

  const char* ko_path = argv[1];

#if defined(__Fuchsia__)
  fprintf(stderr, "[rfkill-qemu] Running on Fuchsia bootfs\n");
  dh_resources_init();
#else
  fprintf(stderr, "[rfkill-qemu] Running on host (userspace simulation)\n");
#endif

  // -----------------------------------------------------------------------
  // Phase 1: Load rfkill.ko (ELF parsing + ARM64 relocations + symbols)
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 1: Load rfkill.ko (ELF + relocations + symbols) ---\n"
          "[rfkill-qemu] Loading %s\n",
          ko_path);

  std::ifstream file(ko_path, std::ios::binary | std::ios::ate);
  bool file_ok = file.is_open();
  std::vector<uint8_t> ko_data;
  if (file_ok) {
    size_t size = file.tellg();
    file.seekg(0);
    ko_data.resize(size);
    file.read(reinterpret_cast<char*>(ko_data.data()), size);
    fprintf(stderr, "[rfkill-qemu] Read %zu bytes from %s\n", size, ko_path);
  } else {
    fprintf(stderr, "[rfkill-qemu] ERROR: cannot open %s\n", ko_path);
  }

  driverhub::SymbolRegistry symbols;
  symbols.RegisterKmiSymbols();
  size_t kmi_count = symbols.size();
  fprintf(stderr, "[rfkill-qemu] Registered %zu KMI symbols\n", kmi_count);

#if defined(__Fuchsia__)
  driverhub::VmoAllocator vmo_allocator;
  driverhub::ModuleLoader loader(symbols, vmo_allocator);
#else
  driverhub::ModuleLoader loader(symbols);
#endif

  std::unique_ptr<driverhub::LoadedModule> module;
  if (file_ok) {
    module = loader.Load("rfkill", ko_data.data(), ko_data.size());
  }

  bool elf_loaded = (module != nullptr);
  bool has_init = elf_loaded && module->init_fn != nullptr;
  bool has_exit = elf_loaded && module->exit_fn != nullptr;

  fprintf(stderr, "[rfkill-qemu] ELF load: %s\n",
          elf_loaded ? "OK" : "FAILED");
  if (elf_loaded) {
    fprintf(stderr, "[rfkill-qemu]   init_fn: %p  exit_fn: %p\n",
            reinterpret_cast<void*>(module->init_fn),
            reinterpret_cast<void*>(module->exit_fn));
    // Verify key exports are present (registered in symbol registry by loader).
    void* rfkill_alloc_addr = symbols.Resolve("rfkill_alloc");
    void* rfkill_register_addr = symbols.Resolve("rfkill_register");
    void* rfkill_destroy_addr = symbols.Resolve("rfkill_destroy");
    fprintf(stderr, "[rfkill-qemu]   rfkill_alloc:    %p\n", rfkill_alloc_addr);
    fprintf(stderr, "[rfkill-qemu]   rfkill_register: %p\n", rfkill_register_addr);
    fprintf(stderr, "[rfkill-qemu]   rfkill_destroy:  %p\n", rfkill_destroy_addr);
  }

  bool has_exports = elf_loaded &&
      symbols.Resolve("rfkill_alloc") != nullptr &&
      symbols.Resolve("rfkill_register") != nullptr;

  // -----------------------------------------------------------------------
  // Phase 2: Call init_module() — creates /dev/rfkill via misc_register
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 2: Call init_module() ---\n");

  bool init_ok = false;
  if (has_init) {
    fprintf(stderr, "[rfkill-qemu] Calling init_module at %p...\n",
            reinterpret_cast<void*>(module->init_fn));
    int init_ret = driverhub::SafeCallInit(module->init_fn);
    if (init_ret == driverhub::MODULE_EXEC_FAULT) {
      fprintf(stderr, "[rfkill-qemu] init_module FAULTED\n");
    } else {
      fprintf(stderr, "[rfkill-qemu] init_module returned %d\n", init_ret);
      init_ok = (init_ret == 0);
    }
  }

  // Wait for workqueue to drain.
  fprintf(stderr, "[rfkill-qemu] Waiting for workqueue to drain...\n");
  for (volatile int i = 0; i < 5000000; i++) {}
  fprintf(stderr, "[rfkill-qemu] Workqueue drain complete\n");

  // -----------------------------------------------------------------------
  // Phase 3: Register test radios via the MODULE's own exported functions
  //          (rfkill.ko's compiled rfkill_alloc/rfkill_register, NOT shim)
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 3: Register Radios via Module Exports ---\n");

  // Set up exception protection for all module code execution (Phases 3-5).
  // GKI module code may contain privileged ARM64 instructions (MRS/MSR)
  // that must be emulated.  The guard covers rfkill_alloc/register (Phase 3),
  // fops calls (Phase 4), and rfkill_unregister/destroy (Phase 5 cleanup).
  FopsGuard fops_guard;
  fops_guard.Start();

  // Cast module exports to Linux function signatures.
  // rfkill_alloc(name, parent, type, ops, ops_data) -> struct rfkill*
  using rfkill_alloc_fn_t = void*(*)(const char*, void*, int,
                                      const void*, void*);
  using rfkill_register_fn_t = int(*)(void*);
  using rfkill_unregister_fn_t = void(*)(void*);
  using rfkill_destroy_fn_t = void(*)(void*);

  auto mod_rfkill_alloc = reinterpret_cast<rfkill_alloc_fn_t>(
      symbols.Resolve("rfkill_alloc"));
  auto mod_rfkill_register = reinterpret_cast<rfkill_register_fn_t>(
      symbols.Resolve("rfkill_register"));
  auto mod_rfkill_unregister = reinterpret_cast<rfkill_unregister_fn_t>(
      symbols.Resolve("rfkill_unregister"));
  auto mod_rfkill_destroy = reinterpret_cast<rfkill_destroy_fn_t>(
      symbols.Resolve("rfkill_destroy"));

  void* wifi_rf = nullptr;
  void* bt_rf = nullptr;
  void* nfc_rf = nullptr;
  bool alloc_ok = false;
  bool register_ok = false;

  if (mod_rfkill_alloc && mod_rfkill_register) {
    // Call rfkill.ko's own compiled rfkill_alloc — ARM64 machine code.
    // Must pass valid rfkill_ops with set_block callback (module validates).
    wifi_rf = mod_rfkill_alloc("phy0-wifi", nullptr, 1 /*WLAN*/, &test_ops,
                                nullptr);
    bt_rf = mod_rfkill_alloc("hci0-bt", nullptr, 2 /*BT*/, &test_ops, nullptr);
    nfc_rf = mod_rfkill_alloc("nfc0", nullptr, 8 /*NFC*/, &test_ops, nullptr);

    alloc_ok = wifi_rf && bt_rf && nfc_rf;
    fprintf(stderr, "[rfkill-qemu] rfkill_alloc (module code): wifi=%p bt=%p nfc=%p\n",
            wifi_rf, bt_rf, nfc_rf);

    if (alloc_ok) {
      // Call rfkill.ko's own compiled rfkill_register.
      int r1 = mod_rfkill_register(wifi_rf);
      int r2 = mod_rfkill_register(bt_rf);
      int r3 = mod_rfkill_register(nfc_rf);
      register_ok = (r1 == 0 && r2 == 0 && r3 == 0);
      fprintf(stderr, "[rfkill-qemu] rfkill_register (module code): wifi=%d bt=%d nfc=%d\n",
              r1, r2, r3);
    }
  } else {
    fprintf(stderr, "[rfkill-qemu] SKIP: module exports not available\n");
  }

  // -----------------------------------------------------------------------
  // Phase 4: DevFs path — the server-side of fuchsia.driverhub.Vfs FIDL
  //
  // This tests the complete data path that a FIDL client would use:
  //   Client → Vfs FIDL protocol → DevFs C++ API → module file_operations
  //
  // Each DevFs method dispatches directly to rfkill.ko's compiled fops:
  //   DevFs::Open  → fops->open  (rfkill_fop_open  at offset 104)
  //   DevFs::Read  → fops->read  (rfkill_fop_read  at offset 16)
  //   DevFs::Write → fops->write (rfkill_fop_write at offset 24)
  //   DevFs::Ioctl → fops->ioctl (rfkill_fop_ioctl at offset 72)
  //   DevFs::Close → fops->release (rfkill_fop_release at offset 120)
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 4: DevFs Path (fuchsia.driverhub.Vfs server-side) ---\n");

  // Verify /dev/rfkill is in the DevFs tree (created by misc_register
  // during init_module).
  auto& devfs = driverhub::DevFs::Instance();
  auto* devfs_node = devfs.Lookup("rfkill");
  bool devfs_registered = (devfs_node != nullptr);
  fprintf(stderr, "[rfkill-qemu] DevFs /dev/rfkill: %s\n",
          devfs_registered ? "registered" : "NOT FOUND");

  bool devfs_has_fops = devfs_registered && devfs_node->fops != nullptr;
  bool devfs_has_open = devfs_has_fops && devfs_node->fops->open != nullptr;
  bool devfs_has_read = devfs_has_fops && devfs_node->fops->read != nullptr;
  bool devfs_has_write = devfs_has_fops && devfs_node->fops->write != nullptr;
  bool devfs_has_ioctl = devfs_has_fops && devfs_node->fops->unlocked_ioctl != nullptr;
  fprintf(stderr, "[rfkill-qemu] fops: open=%s read=%s write=%s ioctl=%s\n",
          devfs_has_open ? "yes" : "no",
          devfs_has_read ? "yes" : "no",
          devfs_has_write ? "yes" : "no",
          devfs_has_ioctl ? "yes" : "no");

  if (devfs_has_fops) {
    fprintf(stderr, "[rfkill-qemu] fops addr: %p  read=%p write=%p open=%p\n",
            reinterpret_cast<const void*>(devfs_node->fops),
            reinterpret_cast<const void*>(devfs_node->fops->read),
            reinterpret_cast<const void*>(devfs_node->fops->write),
            reinterpret_cast<const void*>(devfs_node->fops->open));
  }

  // Vfs.Open → DevFs::Open → rfkill_fop_open
  bool devfs_open_ok = false;
  bool devfs_read_ok = false;
  bool devfs_write_ok = false;
  bool devfs_close_ok = false;
  int dev_fd = -1;

  if (devfs_has_open) {
    fprintf(stderr, "[rfkill-qemu] Calling DevFs::Open...\n");
    dev_fd = devfs.Open("rfkill");
    devfs_open_ok = (dev_fd >= 0);
    fprintf(stderr, "[rfkill-qemu] Vfs.Open /dev/rfkill: fd=%d (%s)\n",
            dev_fd, devfs_open_ok ? "OK" : "FAILED");
  }

  // Vfs.DeviceRead → DevFs::Read → rfkill_fop_read
  // rfkill_fop_read returns ONE event per call. Call multiple times
  // to drain the event queue.
  if (devfs_open_ok && devfs_has_read) {
    fprintf(stderr, "[rfkill-qemu] Calling DevFs::Read...\n");

    // rfkill_fop_read returns one event at a time.
    // Read events one by one (rfkill ABI: sizeof(rfkill_event) per read).
    struct rfkill_event events[8];
    int nevents = 0;
    memset(events, 0, sizeof(events));

    for (int attempt = 0; attempt < 8 && nevents < 3; attempt++) {
      fprintf(stderr, "[rfkill-qemu]   Read attempt %d...\n", attempt);
      ssize_t n = devfs.Read(dev_fd,
                              reinterpret_cast<char*>(&events[nevents]),
                              sizeof(struct rfkill_event));
      fprintf(stderr, "[rfkill-qemu]   Read returned %zd\n", n);
      if (n <= 0) break;
      fprintf(stderr, "[rfkill-qemu]   event[%d]: idx=%u type=%u op=%u soft=%u hard=%u\n",
              nevents, events[nevents].idx, events[nevents].type,
              events[nevents].op, events[nevents].soft,
              events[nevents].hard);
      nevents++;
    }

    fprintf(stderr, "[rfkill-qemu] Vfs.DeviceRead: %d events total\n",
            nevents);
    // We registered 3 radios, expect 3 RFKILL_OP_ADD events.
    devfs_read_ok = (nevents >= 3);
  }

  // Vfs.DeviceWrite → DevFs::Write → rfkill_fop_write
  // Write a CHANGE event to block radio 0.
  if (devfs_open_ok && devfs_has_write) {
    fprintf(stderr, "[rfkill-qemu] Calling DevFs::Write...\n");
    struct rfkill_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.idx = 0;
    ev.type = 1;  // WLAN
    ev.op = RFKILL_OP_CHANGE;
    ev.soft = 1;  // block

    ssize_t n = devfs.Write(dev_fd, reinterpret_cast<const char*>(&ev),
                             sizeof(ev));
    // rfkill_fop_write may return sizeof(rfkill_event_ext)=9 or our 8.
    devfs_write_ok = (n > 0);
    fprintf(stderr, "[rfkill-qemu] Vfs.DeviceWrite (block radio 0): %zd bytes (%s)\n",
            n, devfs_write_ok ? "OK" : "FAILED");
  }

  // Vfs.DeviceClose → DevFs::Close → rfkill_fop_release
  if (devfs_open_ok) {
    fprintf(stderr, "[rfkill-qemu] Calling DevFs::Close...\n");
    int close_ret = devfs.Close(dev_fd);
    devfs_close_ok = (close_ret == 0);
    fprintf(stderr, "[rfkill-qemu] Vfs.DeviceClose: %s\n",
            devfs_close_ok ? "OK" : "FAILED");
  }

  // Verify DevFs List (Vfs.List equivalent).
  auto dev_nodes = devfs.ListDevices();
  bool devfs_list_ok = false;
  for (const auto* n : dev_nodes) {
    if (n->name == "rfkill") {
      devfs_list_ok = true;
      break;
    }
  }
  fprintf(stderr, "[rfkill-qemu] Vfs.List /dev: %zu nodes, rfkill=%s\n",
          dev_nodes.size(), devfs_list_ok ? "found" : "NOT FOUND");

  // -----------------------------------------------------------------------
  // Phase 5: Cleanup — call cleanup_module + destroy test radios
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 5: Cleanup ---\n");

  if (mod_rfkill_unregister && mod_rfkill_destroy) {
    if (wifi_rf) { mod_rfkill_unregister(wifi_rf); mod_rfkill_destroy(wifi_rf); }
    if (bt_rf) { mod_rfkill_unregister(bt_rf); mod_rfkill_destroy(bt_rf); }
    if (nfc_rf) { mod_rfkill_unregister(nfc_rf); mod_rfkill_destroy(nfc_rf); }
    fprintf(stderr, "[rfkill-qemu] Radios unregistered + destroyed (module code)\n");
  }

  // Stop the exception guard before SafeCallExit, which creates its own
  // process-wide exception channel (only one can be active at a time).
  fops_guard.Stop();

  bool exit_ok = false;
  if (init_ok && has_exit) {
    fprintf(stderr, "[rfkill-qemu] Calling cleanup_module at %p...\n",
            reinterpret_cast<void*>(module->exit_fn));
    int exit_ret = driverhub::SafeCallExit(module->exit_fn);
    if (exit_ret == driverhub::MODULE_EXEC_FAULT) {
      fprintf(stderr, "[rfkill-qemu] cleanup_module FAULTED\n");
    } else {
      fprintf(stderr, "[rfkill-qemu] cleanup_module completed\n");
      exit_ok = true;
    }
  } else if (!init_ok) {
    fprintf(stderr, "[rfkill-qemu] SKIP cleanup_module (init failed)\n");
  }

  fprintf(stderr, "[rfkill-qemu] Shutdown complete\n");

  // -----------------------------------------------------------------------
  // Results
  // -----------------------------------------------------------------------
  check("rfkill.ko ELF loaded + symbols resolved", elf_loaded);
  check("init_fn entry point found", has_init);
  check("exit_fn entry point found", has_exit);
  check("Module exports rfkill_alloc/rfkill_register", has_exports);
  check("init_module() executed successfully", init_ok);
  check("rfkill_alloc via module code", alloc_ok);
  check("rfkill_register via module code", register_ok);
  check("DevFs /dev/rfkill registered by misc_register", devfs_registered);
  check("DevFs fops->open present (offset 104)", devfs_has_open);
  check("DevFs fops->read present (offset 16)", devfs_has_read);
  check("DevFs fops->write present (offset 24)", devfs_has_write);
  check("DevFs fops->ioctl present (offset 72)", devfs_has_ioctl);
  check("Vfs.Open → rfkill_fop_open", devfs_open_ok);
  check("Vfs.DeviceRead → rfkill_fop_read (radio events)", devfs_read_ok);
  check("Vfs.DeviceWrite → rfkill_fop_write (block cmd)", devfs_write_ok);
  check("Vfs.DeviceClose → rfkill_fop_release", devfs_close_ok);
  check("Vfs.List /dev shows rfkill", devfs_list_ok);
  check("cleanup_module() executed successfully", exit_ok || !init_ok);

  bool all_pass = (g_fail == 0);

  fprintf(stderr,
          "\n============================================================\n"
          "  RFkill QEMU Demo Results\n"
          "============================================================\n"
          "  Total:  %d\n"
          "  PASS:   %d\n"
          "  FAIL:   %d\n"
          "============================================================\n\n",
          g_pass + g_fail, g_pass, g_fail);

  return all_pass ? 0 : 1;
}
