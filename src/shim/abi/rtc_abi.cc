// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ABI-compatible RTC subsystem shim for loading real GKI .ko modules.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

extern "C" {

#include "src/shim/include/linux/abi/structs.h"

static int rtc_id_counter = 0;

struct rtc_device *devm_rtc_allocate_device(struct device *parent) {
  auto *rtc = static_cast<struct rtc_device *>(
      calloc(1, sizeof(struct rtc_device)));
  if (!rtc)
    return reinterpret_cast<struct rtc_device *>((long)-12);  // -ENOMEM

  rtc->id = rtc_id_counter++;
  char name[32];
  snprintf(name, sizeof(name), "rtc%d", rtc->id);
  rtc->dev.init_name = strdup(name);
  rtc->dev.parent = parent;

  fprintf(stderr, "driverhub: rtc: allocated %s\n", name);
  return rtc;
}

// The real kernel exports __devm_rtc_register_device (the non-underscore
// version is a static inline wrapper in the header).
int __devm_rtc_register_device(void *owner, struct rtc_device *rtc) {
  (void)owner;
  const char *name = rtc->dev.init_name ? rtc->dev.init_name : "rtc?";
  fprintf(stderr, "driverhub: rtc: registered %s (ops=%p)\n",
          name, (void *)rtc->ops);

  if (rtc->ops) {
    fprintf(stderr, "  read_time=%p set_time=%p read_alarm=%p set_alarm=%p\n",
            reinterpret_cast<const void *>(rtc->ops->read_time),
            reinterpret_cast<const void *>(rtc->ops->set_time),
            rtc->ops->read_alarm
                ? reinterpret_cast<const void *>(rtc->ops->read_alarm)
                : nullptr,
            rtc->ops->set_alarm
                ? reinterpret_cast<const void *>(rtc->ops->set_alarm)
                : nullptr);
  }
  return 0;
}

// Also provide the non-underscore name for our shim-compiled modules.
int devm_rtc_register_device(struct rtc_device *rtc) {
  return __devm_rtc_register_device(nullptr, rtc);
}

void rtc_update_irq(struct rtc_device *rtc, unsigned long num,
                    unsigned long events) {
  fprintf(stderr, "driverhub: rtc: IRQ (num=%lu events=0x%lx)\n",
          num, events);
  (void)rtc;
}

void rtc_time64_to_tm(int64_t time, struct rtc_time *tm) {
  time_t t = static_cast<time_t>(time);
  struct std::tm result;
  gmtime_r(&t, &result);
  tm->tm_sec = result.tm_sec;
  tm->tm_min = result.tm_min;
  tm->tm_hour = result.tm_hour;
  tm->tm_mday = result.tm_mday;
  tm->tm_mon = result.tm_mon;
  tm->tm_year = result.tm_year;
  tm->tm_wday = result.tm_wday;
  tm->tm_yday = result.tm_yday;
}

int64_t rtc_tm_to_time64(struct rtc_time *tm) {
  struct std::tm t;
  memset(&t, 0, sizeof(t));
  t.tm_sec = tm->tm_sec;
  t.tm_min = tm->tm_min;
  t.tm_hour = tm->tm_hour;
  t.tm_mday = tm->tm_mday;
  t.tm_mon = tm->tm_mon;
  t.tm_year = tm->tm_year;
  return static_cast<int64_t>(timegm(&t));
}

int64_t ktime_get_real_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return static_cast<int64_t>(ts.tv_sec);
}

}  // extern "C"
