// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end test: loads rtc-test.ko, then exercises the registered RTC ops
// (read_time, set_time) through their function pointers to verify the full
// shim path works from module registration through ops invocation.

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    tests_run++;                                                               \
    if (!(cond)) {                                                             \
      fprintf(stderr, "  FAIL: %s\n", msg);                                    \
    } else {                                                                   \
      fprintf(stderr, "  PASS: %s\n", msg);                                    \
      tests_passed++;                                                          \
    }                                                                          \
  } while (0)

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <rtc-test.ko>\n", argv[0]);
    return 1;
  }

  fprintf(stderr, "\n=== RTC ops end-to-end test ===\n\n");

  driverhub::BusDriver bus;
  auto status = bus.Init();
  if (status != 0) {
    fprintf(stderr, "fatal: bus driver init failed: %d\n", status);
    return 1;
  }

  status = bus.LoadModuleFromFile(argv[1]);
  if (status != 0) {
    fprintf(stderr, "fatal: failed to load %s: %d\n", argv[1], status);
    return 1;
  }

  fprintf(stderr, "\n--- Exercising RTC ops ---\n\n");

  int count = driverhub_rtc_device_count();
  CHECK(count >= 1, "at least one RTC device registered");

  // Exercise rtc0 (uses test_rtc_ops_noalm: read_time + set_time only).
  struct rtc_device *rtc0 = driverhub_rtc_get_device(0);
  CHECK(rtc0 != nullptr, "rtc0 exists");
  CHECK(rtc0->ops != nullptr, "rtc0 has ops");
  CHECK(rtc0->ops->read_time != nullptr, "rtc0 has read_time");
  CHECK(rtc0->ops->set_time != nullptr, "rtc0 has set_time");

  // Read current time through the module's ops.
  struct rtc_time tm;
  memset(&tm, 0, sizeof(tm));
  int ret = rtc0->ops->read_time(rtc0->dev.parent, &tm);
  CHECK(ret == 0, "read_time returns 0");
  CHECK(tm.tm_year >= 124, "read_time year is 2024+");  // tm_year is years since 1900
  fprintf(stderr, "  read_time -> %04d-%02d-%02d %02d:%02d:%02d\n",
          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
          tm.tm_hour, tm.tm_min, tm.tm_sec);

  // Set a known time: 2025-06-15 12:30:00 UTC.
  struct rtc_time set_tm;
  memset(&set_tm, 0, sizeof(set_tm));
  set_tm.tm_year = 125;  // 2025
  set_tm.tm_mon = 5;     // June (0-indexed)
  set_tm.tm_mday = 15;
  set_tm.tm_hour = 12;
  set_tm.tm_min = 30;
  set_tm.tm_sec = 0;

  ret = rtc0->ops->set_time(rtc0->dev.parent, &set_tm);
  CHECK(ret == 0, "set_time returns 0");

  // Read it back — should reflect the offset we just set.
  struct rtc_time read_back;
  memset(&read_back, 0, sizeof(read_back));
  ret = rtc0->ops->read_time(rtc0->dev.parent, &read_back);
  CHECK(ret == 0, "read_time after set returns 0");
  // The rtc-test driver stores an offset; reading back should give ~2025-06-15.
  CHECK(read_back.tm_year == 125, "read_back year is 2025");
  CHECK(read_back.tm_mon == 5, "read_back month is June");
  CHECK(read_back.tm_mday == 15, "read_back day is 15");
  CHECK(read_back.tm_hour == 12, "read_back hour is 12");
  CHECK(read_back.tm_min == 30, "read_back min is 30");
  fprintf(stderr, "  read_back -> %04d-%02d-%02d %02d:%02d:%02d\n",
          read_back.tm_year + 1900, read_back.tm_mon + 1, read_back.tm_mday,
          read_back.tm_hour, read_back.tm_min, read_back.tm_sec);

  // If we have rtc1, it should have alarm ops too.
  if (count >= 2) {
    struct rtc_device *rtc1 = driverhub_rtc_get_device(1);
    CHECK(rtc1 != nullptr, "rtc1 exists");
    CHECK(rtc1->ops->read_alarm != nullptr, "rtc1 has read_alarm");
    CHECK(rtc1->ops->set_alarm != nullptr, "rtc1 has set_alarm");
  }

  fprintf(stderr, "\n--- Results: %d/%d passed ---\n\n", tests_passed, tests_run);

  bus.Shutdown();
  return tests_passed == tests_run ? 0 : 1;
}
