// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_TIME_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_TIME_H_

// KMI shims for Linux kernel time/jiffies APIs.
//
// Provides: jiffies, HZ, msecs_to_jiffies, jiffies_to_msecs,
//           msleep, usleep_range, udelay, ktime_get, ktime_get_real
//
// Backed by clock_gettime / nanosleep in userspace.
// On Fuchsia: zx_clock_get_monotonic / zx_nanosleep.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// HZ = 1000 matches most Android GKI kernels.
#define HZ 1000

typedef int64_t ktime_t;

extern volatile unsigned long jiffies;

unsigned long msecs_to_jiffies(unsigned int m);
unsigned int jiffies_to_msecs(unsigned long j);
void msleep(unsigned int msecs);
void usleep_range(unsigned long min_us, unsigned long max_us);

ktime_t ktime_get(void);
ktime_t ktime_get_real(void);

// time64_to_tm: convert seconds since epoch to broken-down time.
// Used by time_test.ko and other GKI modules.
// The result parameter is a pointer to the Linux kernel's struct tm
// (different from POSIX struct tm). Declared as void* here to avoid
// header conflicts; the .ko module has the correct struct definition.
void time64_to_tm(long long totalsecs, int offset, void *result);

// ktime helpers (nanosecond based).
static inline int64_t ktime_to_ns(ktime_t kt) { return kt; }
static inline int64_t ktime_to_us(ktime_t kt) { return kt / 1000; }
static inline int64_t ktime_to_ms(ktime_t kt) { return kt / 1000000; }
static inline ktime_t ns_to_ktime(int64_t ns) { return ns; }

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_TIME_H_
