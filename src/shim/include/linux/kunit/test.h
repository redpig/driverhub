/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KUnit (Kernel Unit Test) framework — minimal shim for DriverHub.
 *
 * Provides the subset of KUnit APIs used by GKI test modules
 * (time_test.ko, clk-gate_test.ko, input_test.ko, etc.).
 */
#ifndef _LINUX_KUNIT_TEST_H
#define _LINUX_KUNIT_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

struct kunit;
struct kunit_case;
struct kunit_suite;

/* Assertion types. */
enum kunit_assert_type {
  KUNIT_ASSERTION,
  KUNIT_EXPECTATION,
};

/* Binary assertion — used by KUNIT_EXPECT_EQ, KUNIT_ASSERT_EQ, etc. */
struct kunit_binary_assert {
  const char *left_text;
  const char *right_text;
  long long left_value;
  long long right_value;
  const char *operation;
};

/* Assertion format callback type. */
struct kunit_assert;
typedef void (*kunit_assert_format_t)(const struct kunit_assert *assert,
                                      const struct va_format *message,
                                      char *buf, int buf_len);

/* Core assertion function called by KUNIT_EXPECT_* / KUNIT_ASSERT_* macros. */
void __kunit_do_failed_assertion(struct kunit *test,
                                 const void *assert_struct,
                                 enum kunit_assert_type type,
                                 const char *file, int line,
                                 const char *fmt, ...);

/* Abort the current test (longjmp or similar). */
void __kunit_abort(struct kunit *test);

/* Format a binary assertion for display. */
void kunit_binary_assert_format(const struct kunit_assert *assert,
                                const struct va_format *message,
                                char *buf, int buf_len);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* _LINUX_KUNIT_TEST_H */
