// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RTC subsystem shim: provides devm_rtc_allocate_device,
// devm_rtc_register_device, rtc_update_irq, and time conversion helpers.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// We define the structs directly here to avoid header conflicts between
// our linux/ shim headers and the system C++ headers (POSIX timer_delete).

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

extern "C" {

static int rtc_id_counter = 0;

struct rtc_device *devm_rtc_allocate_device(struct device *parent) {
  auto *rtc = static_cast<struct rtc_device *>(
      calloc(1, sizeof(struct rtc_device)));
  if (!rtc)
    return reinterpret_cast<struct rtc_device *>((long)-12);  // -ENOMEM

  rtc->id = rtc_id_counter++;
  snprintf(rtc->name, sizeof(rtc->name), "rtc%d", rtc->id);
  rtc->dev.init_name = rtc->name;
  rtc->dev.parent = parent;

  fprintf(stderr, "driverhub: rtc: allocated %s\n", rtc->name);
  return rtc;
}

int devm_rtc_register_device(struct rtc_device *rtc) {
  fprintf(stderr, "driverhub: rtc: registered %s (read_time=%p, "
          "set_time=%p, read_alarm=%p, set_alarm=%p)\n",
          rtc->name,
          reinterpret_cast<void *>(rtc->ops->read_time),
          reinterpret_cast<void *>(rtc->ops->set_time),
          rtc->ops->read_alarm
              ? reinterpret_cast<void *>(rtc->ops->read_alarm)
              : nullptr,
          rtc->ops->set_alarm
              ? reinterpret_cast<void *>(rtc->ops->set_alarm)
              : nullptr);
  return 0;
}

void rtc_update_irq(struct rtc_device *rtc, unsigned long num,
                    unsigned long events) {
  fprintf(stderr, "driverhub: rtc: %s IRQ (num=%lu events=0x%lx)\n",
          rtc->name, num, events);
}

// Time conversion: seconds since epoch -> broken-down time.
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

// Broken-down time -> seconds since epoch.
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

// Get real wall-clock seconds.
int64_t ktime_get_real_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return static_cast<int64_t>(ts.tv_sec);
}

}  // extern "C"
