// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// KUnit (Kernel Unit Test) framework shim.
//
// Provides stub implementations of the KUnit APIs used by GKI test modules.
// These let DriverHub load and run kunit-based .ko test modules
// (time_test.ko, clk-gate_test.ko, input_test.ko, etc.) for validation.
//
// Symbols provided:
//   __kunit_do_failed_assertion
//   __kunit_abort
//   kunit_binary_assert_format

#include <cstdarg>
#include <cstdio>
#include <cstring>

extern "C" {

struct kunit;
struct kunit_assert;
struct va_format;

enum kunit_assert_type {
  KUNIT_ASSERTION,
  KUNIT_EXPECTATION,
};

void __kunit_do_failed_assertion(struct kunit* test,
                                 const void* assert_struct,
                                 int type,
                                 const char* file, int line,
                                 const char* fmt, ...) {
  (void)test;
  (void)assert_struct;

  const char* type_str = (type == KUNIT_ASSERTION) ? "ASSERTION" : "EXPECTATION";

  fprintf(stderr, "[driverhub][KUNIT] %s FAILED at %s:%d: ",
          type_str, file ? file : "?", line);

  if (fmt) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
  }
  fprintf(stderr, "\n");
}

void __kunit_abort(struct kunit* test) {
  (void)test;
  fprintf(stderr, "[driverhub][KUNIT] test aborted\n");
  // In the real kernel, this does a longjmp. In userspace, we just log it.
  // A real implementation would use setjmp/longjmp to unwind the test.
}

void kunit_binary_assert_format(const struct kunit_assert* assert_data,
                                const struct va_format* message,
                                char* buf, int buf_len) {
  (void)assert_data;
  (void)message;
  if (buf && buf_len > 0) {
    snprintf(buf, static_cast<size_t>(buf_len),
             "[kunit binary assertion failed]");
  }
}

}  // extern "C"
