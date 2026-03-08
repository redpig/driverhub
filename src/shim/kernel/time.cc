// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/time.h"

#include <time.h>
#include <unistd.h>

// On Fuchsia, these would use zx_clock_get_monotonic() and zx_nanosleep().
// For now, POSIX clock functions work for userspace shim validation.

extern "C" {

volatile unsigned long jiffies = 0;

unsigned long msecs_to_jiffies(unsigned int m) {
  return (unsigned long)m * HZ / 1000;
}

unsigned int jiffies_to_msecs(unsigned long j) {
  return (unsigned int)(j * 1000 / HZ);
}

void msleep(unsigned int msecs) {
  struct timespec ts;
  ts.tv_sec = msecs / 1000;
  ts.tv_nsec = (msecs % 1000) * 1000000L;
  nanosleep(&ts, nullptr);
}

void usleep_range(unsigned long min_us, unsigned long max_us) {
  (void)max_us;  // Just sleep for min.
  struct timespec ts;
  ts.tv_sec = min_us / 1000000;
  ts.tv_nsec = (min_us % 1000000) * 1000L;
  nanosleep(&ts, nullptr);
}

ktime_t ktime_get(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

ktime_t ktime_get_real(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// Linux kernel's struct tm layout (different from POSIX struct tm).
// Must match the layout that GKI .ko modules expect.
struct linux_tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  long tm_year;
  int tm_wday;
  int tm_yday;
};

void time64_to_tm(long long totalsecs, int offset, void* result_ptr) {
  auto* result = static_cast<struct linux_tm*>(result_ptr);
  if (!result) return;

  long long total = totalsecs + offset;

  // Days since epoch (1970-01-01).
  long days = static_cast<long>(total / 86400);
  long rem = static_cast<long>(total % 86400);
  if (rem < 0) {
    rem += 86400;
    days--;
  }

  result->tm_hour = static_cast<int>(rem / 3600);
  rem %= 3600;
  result->tm_min = static_cast<int>(rem / 60);
  result->tm_sec = static_cast<int>(rem % 60);

  // Day of week: Jan 1, 1970 was Thursday (4).
  result->tm_wday = static_cast<int>((days + 4) % 7);
  if (result->tm_wday < 0) result->tm_wday += 7;

  // Year calculation.
  long y = 1970;
  while (true) {
    long dyear = 365;
    if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) dyear = 366;
    if (days < dyear) break;
    days -= dyear;
    y++;
  }

  result->tm_year = y - 1900;
  result->tm_yday = static_cast<int>(days);

  // Month calculation.
  static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  int m;
  for (m = 0; m < 11; m++) {
    int md = mdays[m];
    if (m == 1 && leap) md++;
    if (days < md) break;
    days -= md;
  }
  result->tm_mon = m;
  result->tm_mday = static_cast<int>(days) + 1;
}

}  // extern "C"
