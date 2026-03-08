// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RFkill QEMU Demo
//
// End-to-end test of the rfkill subsystem on aarch64 in QEMU:
//
//   Phase 1: Load rfkill.ko via elfldltl — verifies ELF parsing, ARM64
//            relocations, and symbol resolution against the KMI shim.
//   Phase 2: Register test radios through the shim API (WiFi, BT, NFC).
//   Phase 3: Exercise rfkillctl-equivalent operations via the RfkillService
//            API directly (LIST, BLOCK, UNBLOCK, BLOCKALL, UNBLOCKALL,
//            error handling). Uses the service API rather than Unix sockets
//            since AF_UNIX is not available in minimal Fuchsia bootfs.
//   Phase 4: Clean shutdown.
//
// On Fuchsia bootfs: runs via zircon.autorun.boot.

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "src/loader/module_loader.h"
#include "src/shim/subsystem/rfkill.h"
#include "src/symbols/symbol_registry.h"

// Platform-specific allocator.
#if defined(__Fuchsia__)
#include "src/fuchsia/resource_provider.h"
#include "src/fuchsia/vmo_module_loader.h"
#else
#include "src/loader/mmap_allocator.h"
#endif

// Shim rfkill API (extern "C" in rfkill.cc).
extern "C" {
void* shim_rfkill_alloc(const char* name, void* parent, int type,
                         const void* ops, void* ops_data);
int shim_rfkill_register(void* rfkill);
void shim_rfkill_unregister(void* rfkill);
void shim_rfkill_destroy(void* rfkill);
}

namespace {

int g_pass = 0;
int g_fail = 0;

const char* pass_fail(bool ok) { return ok ? "PASS" : "FAIL"; }

void check(const char* desc, bool ok) {
  fprintf(stderr, "  %s: %s\n", pass_fail(ok), desc);
  if (ok) ++g_pass; else ++g_fail;
}

}  // namespace

int main(int argc, char** argv) {
  fprintf(stderr,
          "\n"
          "============================================================\n"
          "  DriverHub RFkill QEMU Demo + rfkillctl Validation\n"
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

  // Read the .ko file.
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

  // Set up symbol registry and loader.
  driverhub::SymbolRegistry symbols;
  symbols.RegisterKmiSymbols();
  size_t kmi_count = symbols.size();
  fprintf(stderr, "[rfkill-qemu] Registered %zu KMI symbols\n", kmi_count);

#if defined(__Fuchsia__)
  driverhub::VmoAllocator allocator;
#else
  driverhub::MmapAllocator allocator;
#endif
  driverhub::ModuleLoader loader(symbols, allocator);

  // Load (parse ELF, resolve symbols, apply relocations) — but do NOT
  // call init_module. This validates the entire loading pipeline for
  // the real GKI aarch64 rfkill.ko.
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
    fprintf(stderr, "[rfkill-qemu]   module name: '%s'\n",
            module->info.name.c_str());
    fprintf(stderr, "[rfkill-qemu]   description: '%s'\n",
            module->info.description.c_str());
    fprintf(stderr, "[rfkill-qemu]   license: '%s'\n",
            module->info.license.c_str());
  }

  // Note: We deliberately skip calling init_module because rfkill.ko's
  // init touches kernel data structures (proc, sysfs internals) that
  // require deeper shim coverage. The ELF load + full symbol resolution
  // validates the loader pipeline; the service layer exercises the
  // rfkillctl client path independently.

  // -----------------------------------------------------------------------
  // Phase 2: Register test radios via the shim API
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 2: Register Test Radios ---\n");

  // Simulate what real WiFi/BT/NFC drivers would do via rfkill API.
  void* wifi_rf = shim_rfkill_alloc("phy0-wifi", nullptr, 1 /*WLAN*/, nullptr,
                                     nullptr);
  void* bt_rf = shim_rfkill_alloc("hci0-bt", nullptr, 2 /*BT*/, nullptr,
                                   nullptr);
  void* nfc_rf = shim_rfkill_alloc("nfc0", nullptr, 8 /*NFC*/, nullptr,
                                    nullptr);

  bool alloc_ok = wifi_rf && bt_rf && nfc_rf;
  fprintf(stderr, "[rfkill-qemu] alloc: wifi=%p bt=%p nfc=%p\n",
          wifi_rf, bt_rf, nfc_rf);

  int r1 = shim_rfkill_register(wifi_rf);
  int r2 = shim_rfkill_register(bt_rf);
  int r3 = shim_rfkill_register(nfc_rf);
  bool register_ok = (r1 == 0 && r2 == 0 && r3 == 0);
  fprintf(stderr, "[rfkill-qemu] register: wifi=%d bt=%d nfc=%d\n",
          r1, r2, r3);

  // -----------------------------------------------------------------------
  // Phase 3: Exercise rfkillctl-equivalent operations via RfkillService API
  // -----------------------------------------------------------------------
  // Note: AF_UNIX is not available in minimal Fuchsia bootfs, so we test
  // the RfkillService directly — the same logic that the IPC server and
  // rfkillctl client exercise, validated on host via test-rfkill-service.sh.
  fprintf(stderr,
          "\n--- Phase 3: RfkillService API Tests (rfkillctl equivalent) ---\n");

  auto& svc = driverhub::RfkillService::Instance();

  // Test: GetRadios (equivalent to rfkillctl list)
  auto radios = svc.GetRadios();
  bool list_ok = (radios.size() == 3);
  fprintf(stderr, "[rfkill-qemu] GetRadios: %zu radios\n", radios.size());

  bool list_wifi = false, list_bt = false, list_nfc = false;
  for (const auto& r : radios) {
    fprintf(stderr, "[rfkill-qemu]   [%u] %s type=%s soft=%s hard=%s\n",
            r.index, r.name.c_str(),
            driverhub::RfkillTypeName(r.type),
            r.state.soft_blocked ? "blocked" : "unblocked",
            r.state.hard_blocked ? "blocked" : "unblocked");
    if (r.name == "phy0-wifi") list_wifi = true;
    if (r.name == "hci0-bt") list_bt = true;
    if (r.name == "nfc0") list_nfc = true;
  }

  // Test: SetSoftBlock (equivalent to rfkillctl block 0)
  bool block_ok = svc.SetSoftBlock(0, true);
  fprintf(stderr, "[rfkill-qemu] SetSoftBlock(0, true): %s\n",
          block_ok ? "OK" : "FAILED");

  // Verify blocked state
  driverhub::RadioInfo info;
  svc.GetRadio(0, &info);
  bool blocked_visible = info.state.soft_blocked;
  fprintf(stderr, "[rfkill-qemu] Radio 0 soft_blocked: %s\n",
          blocked_visible ? "yes" : "no");

  // Test: Unblock (equivalent to rfkillctl unblock 0)
  bool unblock_ok = svc.SetSoftBlock(0, false);
  svc.GetRadio(0, &info);
  bool unblocked = !info.state.soft_blocked;
  fprintf(stderr, "[rfkill-qemu] SetSoftBlock(0, false): %s, state=%s\n",
          unblock_ok ? "OK" : "FAILED",
          unblocked ? "unblocked" : "blocked");

  // Test: SetSoftBlockAll (equivalent to rfkillctl block all — airplane mode)
  svc.SetSoftBlockAll(driverhub::RfkillType::kAll, true);
  radios = svc.GetRadios();
  int blocked_count = 0;
  for (const auto& r : radios) {
    if (r.state.soft_blocked) ++blocked_count;
  }
  bool all_blocked = (blocked_count == 3);
  fprintf(stderr, "[rfkill-qemu] SetSoftBlockAll(all, true): %d/%zu blocked\n",
          blocked_count, radios.size());

  // Test: Unblock all wlan (equivalent to rfkillctl unblock all wlan)
  svc.SetSoftBlockAll(driverhub::RfkillType::kWlan, false);
  svc.GetRadio(0, &info);
  bool wlan_unblocked = !info.state.soft_blocked;
  driverhub::RadioInfo bt_info;
  svc.GetRadio(1, &bt_info);
  bool bt_still_blocked = bt_info.state.soft_blocked;
  fprintf(stderr, "[rfkill-qemu] UnblockAll(wlan): wifi=%s, bt=%s\n",
          wlan_unblocked ? "unblocked" : "blocked",
          bt_still_blocked ? "blocked" : "unblocked");
  bool unblockall_wlan_ok = wlan_unblocked && bt_still_blocked;

  // Test: Block invalid index
  bool err_invalid = !svc.SetSoftBlock(99, true);
  fprintf(stderr, "[rfkill-qemu] SetSoftBlock(99, true): %s (expected fail)\n",
          err_invalid ? "correctly rejected" : "unexpected success");

  // -----------------------------------------------------------------------
  // Phase 4: Cleanup
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 4: Cleanup ---\n");

  shim_rfkill_unregister(wifi_rf);
  shim_rfkill_unregister(bt_rf);
  shim_rfkill_unregister(nfc_rf);
  shim_rfkill_destroy(wifi_rf);
  shim_rfkill_destroy(bt_rf);
  shim_rfkill_destroy(nfc_rf);

  fprintf(stderr, "[rfkill-qemu] Shutdown complete\n");

  // -----------------------------------------------------------------------
  // Results
  // -----------------------------------------------------------------------
  check("rfkill.ko ELF loaded + symbols resolved", elf_loaded);
  check("init_fn entry point found", has_init);
  check("exit_fn entry point found", has_exit);
  check("rfkill_alloc succeeded", alloc_ok);
  check("rfkill_register succeeded", register_ok);
  check("LIST shows 3 radios", list_ok);
  check("LIST shows phy0-wifi", list_wifi);
  check("LIST shows hci0-bt", list_bt);
  check("LIST shows nfc0", list_nfc);
  check("BLOCK individual radio", block_ok);
  check("Blocked state visible in LIST", blocked_visible);
  check("UNBLOCK individual radio", unblock_ok && unblocked);
  check("BLOCKALL (airplane mode)", all_blocked);
  check("UNBLOCKALL wlan type filter", unblockall_wlan_ok);
  check("BLOCK invalid index returns ERR", err_invalid);

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
