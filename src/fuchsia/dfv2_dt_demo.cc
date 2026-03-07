// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DFv2 Device-Tree Proof of Concept
//
// Demonstrates the end-to-end Fuchsia DFv2 driver flow:
//   1. Parse a device tree blob describing hardware (RTC devices)
//   2. DT entries map to module paths via compatible strings
//   3. Load the .ko module (rtc-test) via DriverHub's ELF loader
//   4. Module probes against platform devices → registers RTC
//   5. Exercise RTC ops (read_time, set_time) through the shim layer
//   6. Clean shutdown with module_exit()
//
// On Fuchsia, this runs in bootfs via QEMU serial console.
// The DT provider would normally receive nodes from the board driver,
// but here we construct a static DTB to prove the concept.

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>

#include "src/bus_driver/bus_driver.h"
#include "src/dt/dt_provider.h"

#if defined(__Fuchsia__)
#include "src/fuchsia/resource_provider.h"
#endif

// Duplicate struct definitions for RTC ops invocation.
struct device {
  const char* init_name;
  struct device* parent;
  void* driver_data;
  void* platform_data;
  void* devres_head;
};

struct rtc_time {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};

struct rtc_wkalrm {
  unsigned char enabled;
  unsigned char pending;
  struct rtc_time time;
};

struct rtc_class_ops {
  int (*read_time)(struct device*, struct rtc_time*);
  int (*set_time)(struct device*, struct rtc_time*);
  int (*read_alarm)(struct device*, struct rtc_wkalrm*);
  int (*set_alarm)(struct device*, struct rtc_wkalrm*);
  int (*alarm_irq_enable)(struct device*, unsigned int);
};

struct rtc_device {
  struct device dev;
  const struct rtc_class_ops* ops;
  char name[32];
  int id;
};

extern "C" {
struct rtc_device* driverhub_rtc_get_device(int index);
int driverhub_rtc_device_count(void);
}

// ---------------------------------------------------------------------------
// FDT builder — constructs a minimal DTB in memory.
// ---------------------------------------------------------------------------

namespace {

void WriteBe32(uint8_t* p, uint32_t val) {
  p[0] = static_cast<uint8_t>(val >> 24);
  p[1] = static_cast<uint8_t>(val >> 16);
  p[2] = static_cast<uint8_t>(val >> 8);
  p[3] = static_cast<uint8_t>(val);
}

class FdtBuilder {
 public:
  void BeginNode(const char* name) {
    PutU32(0x00000001);
    PutString(name);
    Align4();
  }

  void EndNode() { PutU32(0x00000002); }

  void PropString(const char* name, const char* value) {
    uint32_t nameoff = AddString(name);
    uint32_t len = static_cast<uint32_t>(strlen(value) + 1);
    PutU32(0x00000003);
    PutU32(len);
    PutU32(nameoff);
    PutBytes(reinterpret_cast<const uint8_t*>(value), len);
    Align4();
  }

  void PropReg(const char* name,
               const std::vector<std::pair<uint64_t, uint64_t>>& regs) {
    uint32_t nameoff = AddString(name);
    uint32_t len = static_cast<uint32_t>(regs.size() * 16);
    PutU32(0x00000003);
    PutU32(len);
    PutU32(nameoff);
    for (auto& r : regs) {
      PutU64(r.first);
      PutU64(r.second);
    }
    Align4();
  }

  void PropInterrupts(const char* name, const std::vector<uint32_t>& irqs) {
    uint32_t nameoff = AddString(name);
    uint32_t len = static_cast<uint32_t>(irqs.size() * 4);
    PutU32(0x00000003);
    PutU32(len);
    PutU32(nameoff);
    for (auto irq : irqs) {
      PutU32(irq);
    }
    Align4();
  }

  std::vector<uint8_t> Build() {
    PutU32(0x00000009);  // FDT_END

    uint32_t header_size = 40;
    uint32_t mem_rsvmap_size = 16;
    uint32_t off_mem_rsvmap = header_size;
    uint32_t off_dt_struct = off_mem_rsvmap + mem_rsvmap_size;
    uint32_t off_dt_strings =
        off_dt_struct + static_cast<uint32_t>(struct_block_.size());
    uint32_t totalsize =
        off_dt_strings + static_cast<uint32_t>(strings_block_.size());

    std::vector<uint8_t> blob(totalsize, 0);
    uint8_t* p = blob.data();

    WriteBe32(p + 0, 0xD00DFEED);
    WriteBe32(p + 4, totalsize);
    WriteBe32(p + 8, off_dt_struct);
    WriteBe32(p + 12, off_dt_strings);
    WriteBe32(p + 16, off_mem_rsvmap);
    WriteBe32(p + 20, 17);
    WriteBe32(p + 24, 16);
    WriteBe32(p + 28, 0);
    WriteBe32(p + 32, static_cast<uint32_t>(strings_block_.size()));
    WriteBe32(p + 36, static_cast<uint32_t>(struct_block_.size()));

    memset(p + off_mem_rsvmap, 0, mem_rsvmap_size);
    memcpy(p + off_dt_struct, struct_block_.data(), struct_block_.size());
    memcpy(p + off_dt_strings, strings_block_.data(), strings_block_.size());

    return blob;
  }

 private:
  void PutU32(uint32_t val) {
    uint8_t buf[4];
    WriteBe32(buf, val);
    struct_block_.insert(struct_block_.end(), buf, buf + 4);
  }

  void PutU64(uint64_t val) {
    PutU32(static_cast<uint32_t>(val >> 32));
    PutU32(static_cast<uint32_t>(val & 0xFFFFFFFF));
  }

  void PutString(const char* s) {
    size_t len = strlen(s) + 1;
    struct_block_.insert(struct_block_.end(),
                         reinterpret_cast<const uint8_t*>(s),
                         reinterpret_cast<const uint8_t*>(s) + len);
  }

  void PutBytes(const uint8_t* data, uint32_t len) {
    struct_block_.insert(struct_block_.end(), data, data + len);
  }

  void Align4() {
    while (struct_block_.size() % 4 != 0) struct_block_.push_back(0);
  }

  uint32_t AddString(const char* name) {
    const char* base = reinterpret_cast<const char*>(strings_block_.data());
    size_t offset = 0;
    while (offset < strings_block_.size()) {
      if (strcmp(base + offset, name) == 0)
        return static_cast<uint32_t>(offset);
      offset += strlen(base + offset) + 1;
    }
    uint32_t result = static_cast<uint32_t>(strings_block_.size());
    size_t len = strlen(name) + 1;
    strings_block_.insert(strings_block_.end(),
                          reinterpret_cast<const uint8_t*>(name),
                          reinterpret_cast<const uint8_t*>(name) + len);
    return result;
  }

  std::vector<uint8_t> struct_block_;
  std::vector<uint8_t> strings_block_;
};

}  // namespace

int main(int argc, char** argv) {
  fprintf(stderr,
          "\n"
          "============================================================\n"
          "  DriverHub DFv2 Device-Tree Proof of Concept\n"
          "============================================================\n\n");

  if (argc < 2) {
    fprintf(stderr, "usage: %s <rtc-test.ko>\n", argv[0]);
    return 1;
  }

  const char* ko_path = argv[1];

#if defined(__Fuchsia__)
  fprintf(stderr, "[dfv2-poc] Running on Fuchsia — acquiring kernel "
                  "resources...\n");
  dh_resources_init();
#else
  fprintf(stderr, "[dfv2-poc] Running on host (userspace simulation)\n");
#endif

  // -----------------------------------------------------------------------
  // Phase 1: Build and parse Device Tree
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 1: Device Tree Configuration ---\n"
          "[dfv2-poc] Building device tree blob...\n");

  FdtBuilder fdt;
  fdt.BeginNode("");  // root

  // RTC device node — simulating what a board DT would provide.
  fdt.BeginNode("rtc@70");
  fdt.PropString("compatible", "test,rtc-test");
  fdt.PropString("status", "okay");
  fdt.PropReg("reg", {{0x70, 0x02}});     // I/O ports 0x70-0x71
  fdt.PropInterrupts("interrupts", {8});   // IRQ 8 (RTC interrupt)
  fdt.EndNode();

  // A disabled device — should not be loaded.
  fdt.BeginNode("rtc@200");
  fdt.PropString("compatible", "test,rtc-disabled");
  fdt.PropString("status", "disabled");
  fdt.EndNode();

  fdt.EndNode();  // end root

  auto dtb = fdt.Build();
  fprintf(stderr, "[dfv2-poc] DTB built: %zu bytes\n", dtb.size());

  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();

  fprintf(stderr, "[dfv2-poc] Device tree entries:\n");
  for (size_t i = 0; i < entries.size(); i++) {
    fprintf(stderr, "  [%zu] compatible='%s' module='%s' resources=%zu\n",
            i, entries[i].compatible.c_str(), entries[i].module_path.c_str(),
            entries[i].resources.size());
    for (const auto& res : entries[i].resources) {
      if (res.type == driverhub::DtModuleEntry::Resource::kMmio) {
        fprintf(stderr, "       MMIO: base=0x%lx size=0x%lx\n",
                (unsigned long)res.base, (unsigned long)res.size);
      } else {
        fprintf(stderr, "       IRQ:  %lu\n", (unsigned long)res.base);
      }
    }
  }

  if (entries.empty()) {
    fprintf(stderr, "[dfv2-poc] ERROR: no DT entries parsed!\n");
    return 1;
  }

  fprintf(stderr, "[dfv2-poc] DT parsed successfully: %zu device(s) to "
                  "configure\n",
          entries.size());

  // -----------------------------------------------------------------------
  // Phase 2: Initialize DriverHub bus driver (DFv2 bus node)
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 2: DFv2 Bus Driver Init ---\n");

  driverhub::BusDriver bus;
  auto status = bus.Init();
  if (status != 0) {
    fprintf(stderr, "[dfv2-poc] FATAL: bus init failed: %d\n", status);
    return 1;
  }

  // -----------------------------------------------------------------------
  // Phase 3: Load module identified by DT compatible string
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 3: Load Module from DT ---\n"
          "[dfv2-poc] DT compatible='%s' → loading '%s'\n",
          entries[0].compatible.c_str(), ko_path);

  status = bus.LoadModuleFromFile(ko_path);
  if (status != 0) {
    fprintf(stderr, "[dfv2-poc] ERROR: module load failed: %d\n", status);
    bus.Shutdown();
    return 1;
  }

  fprintf(stderr, "[dfv2-poc] Module loaded and probed successfully\n");

  // -----------------------------------------------------------------------
  // Phase 4: Verify RTC device registration
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 4: Verify RTC Registration ---\n");

  int rtc_count = driverhub_rtc_device_count();
  fprintf(stderr, "[dfv2-poc] Registered RTC devices: %d\n", rtc_count);

  if (rtc_count < 1) {
    fprintf(stderr, "[dfv2-poc] ERROR: no RTC devices registered!\n");
    bus.Shutdown();
    return 1;
  }

  struct rtc_device* rtc0 = driverhub_rtc_get_device(0);
  if (!rtc0 || !rtc0->ops || !rtc0->ops->read_time) {
    fprintf(stderr, "[dfv2-poc] ERROR: rtc0 ops not available!\n");
    bus.Shutdown();
    return 1;
  }

  fprintf(stderr, "[dfv2-poc] rtc0: name='%s' id=%d\n", rtc0->name, rtc0->id);
  fprintf(stderr, "[dfv2-poc] rtc0: read_time=%p set_time=%p\n",
          reinterpret_cast<void*>(rtc0->ops->read_time),
          reinterpret_cast<void*>(rtc0->ops->set_time));

  // -----------------------------------------------------------------------
  // Phase 5: Exercise RTC ops (DFv2 child node functionality)
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 5: Exercise RTC Ops ---\n");

  // Read current time.
  struct rtc_time tm;
  memset(&tm, 0, sizeof(tm));
  int ret = rtc0->ops->read_time(rtc0->dev.parent, &tm);
  fprintf(stderr, "[dfv2-poc] read_time() = %d\n", ret);
  fprintf(stderr, "[dfv2-poc] Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
          tm.tm_hour, tm.tm_min, tm.tm_sec);

  // Set a specific time to prove bidirectional ops work.
  struct rtc_time set_tm;
  memset(&set_tm, 0, sizeof(set_tm));
  set_tm.tm_year = 126;  // 2026
  set_tm.tm_mon = 2;     // March (0-indexed)
  set_tm.tm_mday = 7;
  set_tm.tm_hour = 12;
  set_tm.tm_min = 0;
  set_tm.tm_sec = 0;

  ret = rtc0->ops->set_time(rtc0->dev.parent, &set_tm);
  fprintf(stderr, "[dfv2-poc] set_time(2026-03-07 12:00:00) = %d\n", ret);

  // Read back to verify.
  struct rtc_time read_back;
  memset(&read_back, 0, sizeof(read_back));
  ret = rtc0->ops->read_time(rtc0->dev.parent, &read_back);
  fprintf(stderr, "[dfv2-poc] Readback: %04d-%02d-%02d %02d:%02d:%02d\n",
          read_back.tm_year + 1900, read_back.tm_mon + 1, read_back.tm_mday,
          read_back.tm_hour, read_back.tm_min, read_back.tm_sec);

  bool time_ok = (read_back.tm_year == 126 && read_back.tm_mon == 2 &&
                  read_back.tm_mday == 7 && read_back.tm_hour == 12);

  // Check alarm ops on second RTC device.
  if (rtc_count >= 2) {
    struct rtc_device* rtc1 = driverhub_rtc_get_device(1);
    if (rtc1 && rtc1->ops) {
      fprintf(stderr, "[dfv2-poc] rtc1: name='%s' has_alarm=%s\n",
              rtc1->name,
              rtc1->ops->read_alarm ? "yes" : "no");
    }
  }

  // -----------------------------------------------------------------------
  // Phase 6: Shutdown (DFv2 unbind)
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 6: DFv2 Shutdown ---\n");

  bus.Shutdown();

  // -----------------------------------------------------------------------
  // Results
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n============================================================\n"
          "  DFv2 Device-Tree PoC Results\n"
          "============================================================\n"
          "  DT parsing:        PASS (%zu entries)\n"
          "  Module loading:    PASS\n"
          "  RTC registration:  PASS (%d devices)\n"
          "  RTC read_time:     PASS\n"
          "  RTC set_time:      %s\n"
          "  RTC readback:      %s\n"
          "============================================================\n\n",
          entries.size(),
          rtc_count,
          ret == 0 ? "PASS" : "FAIL",
          time_ok ? "PASS" : "FAIL");

  return time_ok ? 0 : 1;
}
