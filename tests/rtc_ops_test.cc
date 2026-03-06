// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end test: loads rtc-test.ko, then exercises the registered RTC ops
// (read_time, set_time) through their function pointers to verify the full
// shim path works from module registration through ops invocation.

#include <cstring>

#include <gtest/gtest.h>

#include "src/bus_driver/bus_driver.h"

// Duplicate struct definitions matching the shim's layout (same as rtc.cc).
// We need these to call the ops function pointers.
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

// Path to the RTC test module. Set from command-line arg in main().
static const char *g_rtc_ko_path = nullptr;

class RtcOpsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NE(g_rtc_ko_path, nullptr)
        << "usage: rtc_ops_test <rtc-test.ko>";
    ASSERT_EQ(bus_.Init(), 0) << "bus driver init failed";
    ASSERT_EQ(bus_.LoadModuleFromFile(g_rtc_ko_path), 0)
        << "failed to load " << g_rtc_ko_path;
  }

  void TearDown() override {
    bus_.Shutdown();
  }

  driverhub::BusDriver bus_;
};

TEST_F(RtcOpsTest, AtLeastOneDevice) {
  EXPECT_GE(driverhub_rtc_device_count(), 1);
}

TEST_F(RtcOpsTest, Rtc0Exists) {
  struct rtc_device *rtc0 = driverhub_rtc_get_device(0);
  ASSERT_NE(rtc0, nullptr);
  ASSERT_NE(rtc0->ops, nullptr);
  EXPECT_NE(rtc0->ops->read_time, nullptr);
  EXPECT_NE(rtc0->ops->set_time, nullptr);
}

TEST_F(RtcOpsTest, ReadTime) {
  struct rtc_device *rtc0 = driverhub_rtc_get_device(0);
  ASSERT_NE(rtc0, nullptr);

  struct rtc_time tm;
  memset(&tm, 0, sizeof(tm));
  int ret = rtc0->ops->read_time(rtc0->dev.parent, &tm);
  EXPECT_EQ(ret, 0);
  EXPECT_GE(tm.tm_year, 124) << "year should be 2024+";
}

TEST_F(RtcOpsTest, SetAndReadBackTime) {
  struct rtc_device *rtc0 = driverhub_rtc_get_device(0);
  ASSERT_NE(rtc0, nullptr);

  // Set a known time: 2025-06-15 12:30:00 UTC.
  struct rtc_time set_tm;
  memset(&set_tm, 0, sizeof(set_tm));
  set_tm.tm_year = 125;  // 2025
  set_tm.tm_mon = 5;     // June (0-indexed)
  set_tm.tm_mday = 15;
  set_tm.tm_hour = 12;
  set_tm.tm_min = 30;
  set_tm.tm_sec = 0;

  ASSERT_EQ(rtc0->ops->set_time(rtc0->dev.parent, &set_tm), 0);

  // Read it back — should reflect the offset we just set.
  struct rtc_time read_back;
  memset(&read_back, 0, sizeof(read_back));
  ASSERT_EQ(rtc0->ops->read_time(rtc0->dev.parent, &read_back), 0);

  EXPECT_EQ(read_back.tm_year, 125);
  EXPECT_EQ(read_back.tm_mon, 5);
  EXPECT_EQ(read_back.tm_mday, 15);
  EXPECT_EQ(read_back.tm_hour, 12);
  EXPECT_EQ(read_back.tm_min, 30);
}

TEST_F(RtcOpsTest, Rtc1HasAlarmOps) {
  int count = driverhub_rtc_device_count();
  if (count < 2) {
    GTEST_SKIP() << "need at least 2 RTC devices for alarm test";
  }

  struct rtc_device *rtc1 = driverhub_rtc_get_device(1);
  ASSERT_NE(rtc1, nullptr);
  EXPECT_NE(rtc1->ops->read_alarm, nullptr);
  EXPECT_NE(rtc1->ops->set_alarm, nullptr);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc >= 2) {
    g_rtc_ko_path = argv[1];
  }
  return RUN_ALL_TESTS();
}
