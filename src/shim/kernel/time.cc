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

}  // extern "C"
