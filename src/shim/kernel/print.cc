// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/print.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// KMI print shims: map Linux kernel logging to stderr.
// On Fuchsia, these would map to FX_LOG macros via the syslog library.

// Parse the KERN_<level> prefix from a format string.
// Returns the format string past the prefix, or the original if no prefix.
static const char* skip_log_level(const char* fmt, int* level) {
  if (fmt[0] == '<' && fmt[1] >= '0' && fmt[1] <= '7' && fmt[2] == '>') {
    *level = fmt[1] - '0';
    return fmt + 3;
  }
  *level = 6;  // Default to KERN_INFO
  return fmt;
}

static const char* level_tag(int level) {
  switch (level) {
    case 0: return "EMERG";
    case 1: return "ALERT";
    case 2: return "CRIT";
    case 3: return "ERROR";
    case 4: return "WARN";
    case 5: return "NOTICE";
    case 6: return "INFO";
    case 7: return "DEBUG";
    default: return "???";
  }
}

extern "C" {

int printk(const char* fmt, ...) {
  int level;
  const char* real_fmt = skip_log_level(fmt, &level);

  fprintf(stderr, "[driverhub][%s] ", level_tag(level));

  va_list args;
  va_start(args, fmt);
  int ret = vfprintf(stderr, real_fmt, args);
  va_end(args);

  // Ensure newline termination (Linux printk usually includes \n).
  size_t len = strlen(real_fmt);
  if (len > 0 && real_fmt[len - 1] != '\n') {
    fputc('\n', stderr);
  }

  return ret;
}

// dev_* functions include the device name when available.
// For now, struct device is opaque — we just print without device context.

void dev_err(const struct device* dev, const char* fmt, ...) {
  (void)dev;
  fprintf(stderr, "[driverhub][ERROR] ");
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void dev_warn(const struct device* dev, const char* fmt, ...) {
  (void)dev;
  fprintf(stderr, "[driverhub][WARN] ");
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void dev_info(const struct device* dev, const char* fmt, ...) {
  (void)dev;
  fprintf(stderr, "[driverhub][INFO] ");
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void dev_dbg(const struct device* dev, const char* fmt, ...) {
  (void)dev;
  fprintf(stderr, "[driverhub][DEBUG] ");
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

}  // extern "C"
