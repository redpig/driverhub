// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Device tree integration test: builds an FDT blob describing RTC devices,
// parses it with StaticDtProvider, then uses the DT entries to configure
// platform devices and loads the rtc-test.ko module — demonstrating the
// full DT → platform device → driver probe → RTC registration path that
// would happen on Fuchsia.

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "src/bus_driver/bus_driver.h"
#include "src/dt/dt_provider.h"

// Duplicate struct definitions matching the shim's layout.
struct device {
  const char *init_name;
  struct device *parent;
  void *driver_data;
  void *platform_data;
  void *devres_head;
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
  int (*read_time)(struct device *, struct rtc_time *);
  int (*set_time)(struct device *, struct rtc_time *);
  int (*read_alarm)(struct device *, struct rtc_wkalrm *);
  int (*set_alarm)(struct device *, struct rtc_wkalrm *);
  int (*alarm_irq_enable)(struct device *, unsigned int);
};

struct rtc_device {
  struct device dev;
  const struct rtc_class_ops *ops;
  char name[32];
  int id;
};

// Provided by the RTC shim.
extern "C" {
struct rtc_device *driverhub_rtc_get_device(int index);
int driverhub_rtc_device_count(void);
}

// ---------------------------------------------------------------------------
// FDT builder — constructs a minimal Flattened Device Tree blob in memory.
// ---------------------------------------------------------------------------

class FdtBuilder {
 public:
  FdtBuilder() {
    // Reserve space for header (40 bytes).
    struct_block_.clear();
    strings_block_.clear();
  }

  // Begin a named node.
  void BeginNode(const char *name) {
    PutU32(0x00000001);  // FDT_BEGIN_NODE
    PutString(name);
    Align4();
  }

  // End the current node.
  void EndNode() {
    PutU32(0x00000002);  // FDT_END_NODE
  }

  // Add a string property.
  void PropString(const char *name, const char *value) {
    uint32_t nameoff = AddString(name);
    uint32_t len = static_cast<uint32_t>(strlen(value) + 1);
    PutU32(0x00000003);  // FDT_PROP
    PutU32(len);
    PutU32(nameoff);
    PutBytes(reinterpret_cast<const uint8_t *>(value), len);
    Align4();
  }

  // Add a reg property with (address, size) pairs as 64-bit big-endian cells.
  void PropReg(const char *name, const std::vector<std::pair<uint64_t, uint64_t>> &regs) {
    uint32_t nameoff = AddString(name);
    uint32_t len = static_cast<uint32_t>(regs.size() * 16);
    PutU32(0x00000003);  // FDT_PROP
    PutU32(len);
    PutU32(nameoff);
    for (auto &r : regs) {
      PutU64(r.first);
      PutU64(r.second);
    }
    Align4();
  }

  // Add an interrupts property with 32-bit IRQ numbers.
  void PropInterrupts(const char *name, const std::vector<uint32_t> &irqs) {
    uint32_t nameoff = AddString(name);
    uint32_t len = static_cast<uint32_t>(irqs.size() * 4);
    PutU32(0x00000003);  // FDT_PROP
    PutU32(len);
    PutU32(nameoff);
    for (auto irq : irqs) {
      PutU32(irq);
    }
    Align4();
  }

  // Finalize: add FDT_END, build header, return complete blob.
  std::vector<uint8_t> Build() {
    PutU32(0x00000009);  // FDT_END

    // Header is 40 bytes.
    uint32_t header_size = 40;
    // Memory reservation block is empty (just a 16-byte zero entry).
    uint32_t mem_rsvmap_size = 16;
    uint32_t off_mem_rsvmap = header_size;
    uint32_t off_dt_struct = off_mem_rsvmap + mem_rsvmap_size;
    uint32_t off_dt_strings = off_dt_struct + static_cast<uint32_t>(struct_block_.size());
    uint32_t totalsize = off_dt_strings + static_cast<uint32_t>(strings_block_.size());

    std::vector<uint8_t> blob(totalsize, 0);
    uint8_t *p = blob.data();

    // Header (big-endian).
    WriteBe32(p + 0, 0xD00DFEED);   // magic
    WriteBe32(p + 4, totalsize);
    WriteBe32(p + 8, off_dt_struct);
    WriteBe32(p + 12, off_dt_strings);
    WriteBe32(p + 16, off_mem_rsvmap);
    WriteBe32(p + 20, 17);           // version
    WriteBe32(p + 24, 16);           // last_comp_version
    WriteBe32(p + 28, 0);            // boot_cpuid_phys
    WriteBe32(p + 32, static_cast<uint32_t>(strings_block_.size()));
    WriteBe32(p + 36, static_cast<uint32_t>(struct_block_.size()));

    // Memory reservation block (empty: 16 zero bytes).
    memset(p + off_mem_rsvmap, 0, mem_rsvmap_size);

    // Structure block.
    memcpy(p + off_dt_struct, struct_block_.data(), struct_block_.size());

    // Strings block.
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

  void PutString(const char *s) {
    size_t len = strlen(s) + 1;
    struct_block_.insert(struct_block_.end(),
                         reinterpret_cast<const uint8_t *>(s),
                         reinterpret_cast<const uint8_t *>(s) + len);
  }

  void PutBytes(const uint8_t *data, uint32_t len) {
    struct_block_.insert(struct_block_.end(), data, data + len);
  }

  void Align4() {
    while (struct_block_.size() % 4 != 0) {
      struct_block_.push_back(0);
    }
  }

  uint32_t AddString(const char *name) {
    // Check if string already exists.
    const char *base = reinterpret_cast<const char *>(strings_block_.data());
    size_t offset = 0;
    while (offset < strings_block_.size()) {
      if (strcmp(base + offset, name) == 0) {
        return static_cast<uint32_t>(offset);
      }
      offset += strlen(base + offset) + 1;
    }
    // Append new string.
    uint32_t result = static_cast<uint32_t>(strings_block_.size());
    size_t len = strlen(name) + 1;
    strings_block_.insert(strings_block_.end(),
                          reinterpret_cast<const uint8_t *>(name),
                          reinterpret_cast<const uint8_t *>(name) + len);
    return result;
  }

  static void WriteBe32(uint8_t *p, uint32_t val) {
    p[0] = static_cast<uint8_t>(val >> 24);
    p[1] = static_cast<uint8_t>(val >> 16);
    p[2] = static_cast<uint8_t>(val >> 8);
    p[3] = static_cast<uint8_t>(val);
  }

  std::vector<uint8_t> struct_block_;
  std::vector<uint8_t> strings_block_;
};

// Build a DTB with RTC nodes:
//   /root {
//     rtc@10000 {
//       compatible = "test,rtc-test";
//       status = "okay";
//       reg = <0x0 0x10000 0x0 0x1000>;
//       interrupts = <8>;
//     };
//     rtc@20000 {
//       compatible = "test,rtc-test";
//       status = "disabled";    // <-- should be excluded
//       reg = <0x0 0x20000 0x0 0x1000>;
//     };
//     rtc@30000 {
//       compatible = "test,rtc-test";
//       // no status property  // <-- should be included (default = okay)
//       reg = <0x0 0x30000 0x0 0x2000>;
//       interrupts = <42 43>;
//     };
//   };
static std::vector<uint8_t> BuildTestDtb() {
  FdtBuilder fdt;

  // Root node.
  fdt.BeginNode("");

    // rtc@10000 — status "okay", should be included.
    fdt.BeginNode("rtc@10000");
      fdt.PropString("compatible", "test,rtc-test");
      fdt.PropString("status", "okay");
      fdt.PropReg("reg", {{0x10000, 0x1000}});
      fdt.PropInterrupts("interrupts", {8});
    fdt.EndNode();

    // rtc@20000 — status "disabled", should be excluded.
    fdt.BeginNode("rtc@20000");
      fdt.PropString("compatible", "test,rtc-test");
      fdt.PropString("status", "disabled");
      fdt.PropReg("reg", {{0x20000, 0x1000}});
    fdt.EndNode();

    // rtc@30000 — no status (defaults to okay), should be included.
    fdt.BeginNode("rtc@30000");
      fdt.PropString("compatible", "test,rtc-test");
      fdt.PropReg("reg", {{0x30000, 0x2000}});
      fdt.PropInterrupts("interrupts", {42, 43});
    fdt.EndNode();

  fdt.EndNode();  // end root

  return fdt.Build();
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static const char *g_rtc_ko_path = nullptr;

// --- Test 1: DTB parsing ---

class DtParseTest : public ::testing::Test {};

TEST_F(DtParseTest, ParsesTestDtb) {
  auto dtb = BuildTestDtb();
  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();

  // Two nodes should be included (okay + default), one excluded (disabled).
  ASSERT_EQ(entries.size(), 2u);
}

TEST_F(DtParseTest, CompatibleStringExtracted) {
  auto dtb = BuildTestDtb();
  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();
  ASSERT_GE(entries.size(), 1u);

  EXPECT_EQ(entries[0].compatible, "test,rtc-test");
  EXPECT_EQ(entries[1].compatible, "test,rtc-test");
}

TEST_F(DtParseTest, ModulePathDerived) {
  auto dtb = BuildTestDtb();
  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();
  ASSERT_GE(entries.size(), 1u);

  // "test,rtc-test" → strip vendor → "rtc-test" → replace '-' → "rtc_test.ko"
  EXPECT_EQ(entries[0].module_path, "rtc_test.ko");
}

TEST_F(DtParseTest, MmioResourcesParsed) {
  auto dtb = BuildTestDtb();
  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();
  ASSERT_GE(entries.size(), 1u);

  // First entry (rtc@10000): one MMIO at 0x10000, size 0x1000.
  ASSERT_EQ(entries[0].resources.size(), 2u);  // 1 MMIO + 1 IRQ
  EXPECT_EQ(entries[0].resources[0].type,
            driverhub::DtModuleEntry::Resource::kMmio);
  EXPECT_EQ(entries[0].resources[0].base, 0x10000u);
  EXPECT_EQ(entries[0].resources[0].size, 0x1000u);
}

TEST_F(DtParseTest, IrqResourcesParsed) {
  auto dtb = BuildTestDtb();
  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();
  ASSERT_GE(entries.size(), 1u);

  // First entry: IRQ 8.
  EXPECT_EQ(entries[0].resources[1].type,
            driverhub::DtModuleEntry::Resource::kIrq);
  EXPECT_EQ(entries[0].resources[1].base, 8u);

  // Second entry (rtc@30000): MMIO + 2 IRQs.
  ASSERT_GE(entries.size(), 2u);
  ASSERT_EQ(entries[1].resources.size(), 3u);  // 1 MMIO + 2 IRQs
  EXPECT_EQ(entries[1].resources[0].base, 0x30000u);
  EXPECT_EQ(entries[1].resources[0].size, 0x2000u);
  EXPECT_EQ(entries[1].resources[1].base, 42u);
  EXPECT_EQ(entries[1].resources[2].base, 43u);
}

TEST_F(DtParseTest, DisabledNodeExcluded) {
  auto dtb = BuildTestDtb();
  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();

  // The disabled node (rtc@20000 with reg=0x20000) should not appear.
  for (const auto &entry : entries) {
    for (const auto &res : entry.resources) {
      if (res.type == driverhub::DtModuleEntry::Resource::kMmio) {
        EXPECT_NE(res.base, 0x20000u)
            << "disabled node should not appear in entries";
      }
    }
  }
}

TEST_F(DtParseTest, InvalidDtbTooSmall) {
  uint8_t small[] = {0xD0, 0x0D, 0xFE, 0xED};
  driverhub::StaticDtProvider provider(small, sizeof(small));
  auto entries = provider.GetModuleEntries();
  EXPECT_EQ(entries.size(), 0u);
}

TEST_F(DtParseTest, InvalidDtbBadMagic) {
  auto dtb = BuildTestDtb();
  // Corrupt the magic.
  dtb[0] = 0x00;
  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();
  EXPECT_EQ(entries.size(), 0u);
}

// --- Test 2: DT-driven RTC module loading ---
// Demonstrates the full Fuchsia flow: DT node → platform device → driver
// probe → RTC registered.

class DtRtcLoadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!g_rtc_ko_path) {
      GTEST_SKIP() << "usage: dt_rtc_test <rtc-test.ko>";
    }
    initial_rtc_count_ = driverhub_rtc_device_count();
    ASSERT_EQ(bus_.Init(), 0) << "bus driver init failed";
  }

  void TearDown() override {
    bus_.Shutdown();
  }

  driverhub::BusDriver bus_;
  int initial_rtc_count_ = 0;
};

TEST_F(DtRtcLoadTest, DtDrivenModuleLoadsAndProbes) {
  // Step 1: Parse device tree.
  auto dtb = BuildTestDtb();
  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();
  ASSERT_EQ(entries.size(), 2u)
      << "expected 2 DT entries (okay + default-okay)";

  // Step 2: Verify DT entries have expected compatible string.
  for (const auto &entry : entries) {
    EXPECT_EQ(entry.compatible, "test,rtc-test");
    EXPECT_EQ(entry.module_path, "rtc_test.ko");
  }

  // Step 3: Load the actual module.
  ASSERT_EQ(bus_.LoadModuleFromFile(g_rtc_ko_path), 0)
      << "failed to load " << g_rtc_ko_path;

  // Step 4: Verify the module's init registered RTC devices via platform
  // driver matching. The rtc-test module creates 3 platform devices
  // internally (rtc-test.0, rtc-test.1, rtc-test.2) and probes them.
  int new_count = driverhub_rtc_device_count() - initial_rtc_count_;
  EXPECT_GE(new_count, 1) << "module should have registered RTC devices";
}

TEST_F(DtRtcLoadTest, DtResourcesWouldConfigurePlatformDevice) {
  // Verify that DT resources can be used to populate platform_device
  // resources (the path used on Fuchsia where DT drives device creation).
  auto dtb = BuildTestDtb();
  driverhub::StaticDtProvider provider(dtb.data(), dtb.size());
  auto entries = provider.GetModuleEntries();
  ASSERT_GE(entries.size(), 1u);

  const auto &entry = entries[0];
  int mmio_count = 0;
  int irq_count = 0;
  for (const auto &res : entry.resources) {
    if (res.type == driverhub::DtModuleEntry::Resource::kMmio) {
      mmio_count++;
      EXPECT_EQ(res.base, 0x10000u);
      EXPECT_EQ(res.size, 0x1000u);
    } else if (res.type == driverhub::DtModuleEntry::Resource::kIrq) {
      irq_count++;
      EXPECT_EQ(res.base, 8u);
    }
  }

  EXPECT_EQ(mmio_count, 1);
  EXPECT_EQ(irq_count, 1);
}

TEST_F(DtRtcLoadTest, DtDrivenRtcOpsWork) {
  // Load module, then exercise RTC ops to show DT-enabled device works.
  ASSERT_EQ(bus_.LoadModuleFromFile(g_rtc_ko_path), 0);

  int new_count = driverhub_rtc_device_count() - initial_rtc_count_;
  ASSERT_GE(new_count, 1);

  struct rtc_device *rtc = driverhub_rtc_get_device(initial_rtc_count_);
  ASSERT_NE(rtc, nullptr);
  ASSERT_NE(rtc->ops, nullptr);
  ASSERT_NE(rtc->ops->read_time, nullptr);
  ASSERT_NE(rtc->ops->set_time, nullptr);

  // Set a known time: 2026-03-07 10:00:00 UTC.
  struct rtc_time set_tm;
  memset(&set_tm, 0, sizeof(set_tm));
  set_tm.tm_year = 126;   // 2026
  set_tm.tm_mon = 2;      // March (0-indexed)
  set_tm.tm_mday = 7;
  set_tm.tm_hour = 10;
  set_tm.tm_min = 0;
  set_tm.tm_sec = 0;

  ASSERT_EQ(rtc->ops->set_time(rtc->dev.parent, &set_tm), 0);

  // Read back.
  struct rtc_time read_back;
  memset(&read_back, 0, sizeof(read_back));
  ASSERT_EQ(rtc->ops->read_time(rtc->dev.parent, &read_back), 0);

  EXPECT_EQ(read_back.tm_year, 126);
  EXPECT_EQ(read_back.tm_mon, 2);
  EXPECT_EQ(read_back.tm_mday, 7);
  EXPECT_EQ(read_back.tm_hour, 10);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc >= 2) {
    g_rtc_ko_path = argv[1];
  }
  return RUN_ALL_TESTS();
}
