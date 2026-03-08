// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_BUG_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_BUG_H_

// KMI shims for Linux kernel BUG/WARN/panic APIs.
//
// In the Linux kernel, BUG() and panic() cause a kernel oops/panic.
// In DriverHub, these are intercepted:
//   - WARN() logs a warning with a backtrace and continues.
//   - BUG() logs an error and tears down only the affected module.
//   - panic() logs an error and tears down only the affected module.
//
// On Fuchsia, these would integrate with the crash reporting framework.

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// BUG() — fatal error, tears down the current module.
// In userspace shim: logs and aborts (in production, would isolate to module).
#define BUG() do { \
  driverhub_bug(__FILE__, __LINE__, __func__); \
} while(0)

#define BUG_ON(cond) do { \
  if (__builtin_expect(!!(cond), 0)) BUG(); \
} while(0)

// WARN() — non-fatal warning, logs and continues.
#define WARN(cond, fmt, ...) ({ \
  int __ret = !!(cond); \
  if (__builtin_expect(__ret, 0)) \
    driverhub_warn(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
  __ret; \
})

#define WARN_ON(cond) WARN(cond, "WARN_ON(" #cond ")")

#define WARN_ON_ONCE(cond) ({ \
  static int __warned = 0; \
  int __ret = !!(cond); \
  if (__builtin_expect(__ret, 0) && !__warned) { \
    __warned = 1; \
    driverhub_warn(__FILE__, __LINE__, __func__, "WARN_ON_ONCE(" #cond ")"); \
  } \
  __ret; \
})

// panic() — fatal, tears down the module.
void panic(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

// Internal helpers (called by macros above).
void driverhub_bug(const char *file, int line, const char *func)
    __attribute__((noreturn));
void driverhub_warn(const char *file, int line, const char *func,
                    const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

// Stack protector — needed by GKI modules compiled with -fstack-protector.
void __stack_chk_fail(void) __attribute__((noreturn));
extern unsigned long __stack_chk_guard;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_BUG_H_
