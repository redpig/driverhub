// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/bug.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// BUG/WARN/panic implementation.
//
// In production Fuchsia usage, BUG() and panic() would tear down only the
// affected module's DFv2 child node rather than the entire process. This
// requires cooperation with the module node's isolation mechanism.
//
// For the userspace shim, we log and abort (BUG/panic) or log and continue
// (WARN).

extern "C" {

void driverhub_bug(const char* file, int line, const char* func) {
  fprintf(stderr,
          "\n[driverhub][BUG] %s:%d in %s\n"
          "  Module hit BUG() — tearing down.\n\n",
          file, line, func);
  // In production, this would signal the module node to tear down.
  // For now, abort to make the failure visible during testing.
  abort();
}

void driverhub_warn(const char* file, int line, const char* func,
                    const char* fmt, ...) {
  fprintf(stderr, "[driverhub][WARN] %s:%d in %s: ", file, line, func);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  // WARN continues execution — it's non-fatal.
}

void panic(const char* fmt, ...) {
  fprintf(stderr, "\n[driverhub][PANIC] ");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n  Module hit panic() — tearing down.\n\n");
  abort();
}

}  // extern "C"
