// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementations of C library-level and fundamental kernel symbols that
// GKI modules expect. These are the most commonly needed symbols across
// the 192 GKI system_dlkm modules.

#include "src/shim/kernel/libc_stubs.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>

extern "C" {

// strscpy: safe copy with NUL termination guarantee.
size_t strscpy(char *dest, const char *src, size_t count) {
  if (count == 0) return (size_t)-1;  // -E2BIG

  size_t i;
  for (i = 0; i < count - 1 && src[i]; i++) {
    dest[i] = src[i];
  }
  dest[i] = '\0';

  if (src[i]) return (size_t)-1;  // -E2BIG, string was truncated
  return i;
}

// kmemdup: allocate and copy memory.
void *kmemdup(const void *src, size_t len, unsigned int gfp) {
  (void)gfp;
  void *p = malloc(len);
  if (p) memcpy(p, src, len);
  return p;
}

// sort: kernel sort (uses stdlib qsort).
void sort(void *base, size_t num, size_t size,
          int (*cmp)(const void *, const void *),
          void (*swap)(void *, void *, size_t)) {
  (void)swap;  // qsort handles swapping internally.
  qsort(base, num, size, cmp);
}

// refcount_warn_saturate: just warn (don't crash).
void refcount_warn_saturate(void *r, int old) {
  fprintf(stderr, "driverhub: refcount_warn_saturate: old=%d\n", old);
}

// RCU stubs — no-ops in single-threaded userspace context.
void __rcu_read_lock(void) {}
void __rcu_read_unlock(void) {}
void synchronize_rcu(void) {}
void call_rcu(void *head, void (*func)(void *)) {
  // In userspace, just call immediately.
  func(head);
}
void rcu_barrier(void) {}

// Preemption stubs.
void preempt_schedule_notrace(void) {}
void preempt_schedule(void) {}

// BH spinlock variants — no-ops in userspace.
void _raw_spin_lock_bh(void *lock) { (void)lock; }
void _raw_spin_unlock_bh(void *lock) { (void)lock; }
void _raw_read_lock(void *lock) { (void)lock; }
void _raw_read_unlock(void *lock) { (void)lock; }
void _raw_write_lock_bh(void *lock) { (void)lock; }
void _raw_write_unlock_bh(void *lock) { (void)lock; }
void _raw_read_lock_bh(void *lock) { (void)lock; }
void _raw_read_unlock_bh(void *lock) { (void)lock; }

// Module reference counting — no-ops.
int try_module_get(void *module) {
  (void)module;
  return 1;  // Success.
}
void module_put(void *module) { (void)module; }

// Per-CPU variable — always CPU 0 in userspace.
unsigned int cpu_number = 0;

// Random bytes.
void get_random_bytes(void *buf, size_t nbytes) {
  // Simple PRNG for userspace; not cryptographically secure.
  unsigned char *p = (unsigned char *)buf;
  for (size_t i = 0; i < nbytes; i++) {
    p[i] = (unsigned char)(rand() & 0xFF);
  }
}

// dev_* logging functions.
void _dev_err(const void *dev, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[driverhub:err] ");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

void _dev_warn(const void *dev, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[driverhub:warn] ");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

void _dev_info(const void *dev, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[driverhub:info] ");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

// Wait queue helpers.
void add_wait_queue(void *wq_head, void *wq_entry) {
  (void)wq_head;
  (void)wq_entry;
}

void remove_wait_queue(void *wq_head, void *wq_entry) {
  (void)wq_head;
  (void)wq_entry;
}

// kstrto* functions.
int kstrtoint(const char *s, unsigned int base, int *res) {
  char *end;
  long val = strtol(s, &end, base);
  if (end == s || *end != '\0') return -22;  // -EINVAL
  *res = (int)val;
  return 0;
}

int kstrtouint(const char *s, unsigned int base, unsigned int *res) {
  char *end;
  unsigned long val = strtoul(s, &end, base);
  if (end == s || *end != '\0') return -22;  // -EINVAL
  *res = (unsigned int)val;
  return 0;
}

int kstrtol(const char *s, unsigned int base, long *res) {
  char *end;
  long val = strtol(s, &end, base);
  if (end == s || *end != '\0') return -22;
  *res = val;
  return 0;
}

int kstrtoul(const char *s, unsigned int base, unsigned long *res) {
  char *end;
  unsigned long val = strtoul(s, &end, base);
  if (end == s || *end != '\0') return -22;
  *res = val;
  return 0;
}

int kstrtobool(const char *s, int *res) {
  if (!s) return -22;
  switch (s[0]) {
    case 'y': case 'Y': case '1':
      *res = 1; return 0;
    case 'n': case 'N': case '0':
      *res = 0; return 0;
    default:
      return -22;
  }
}

// simple_strto* wrappers.
long simple_strtol(const char *cp, char **endp, unsigned int base) {
  return strtol(cp, endp, base);
}

unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base) {
  return strtoul(cp, endp, base);
}

long long simple_strtoll(const char *cp, char **endp, unsigned int base) {
  return strtoll(cp, endp, base);
}

unsigned long long simple_strtoull(const char *cp, char **endp,
                                    unsigned int base) {
  return strtoull(cp, endp, base);
}

// full_name_hash: simple hash for pathnames.
unsigned int full_name_hash(const void *salt, const char *name,
                             unsigned int len) {
  (void)salt;
  unsigned int hash = 0;
  for (unsigned int i = 0; i < len; i++) {
    hash = hash * 31 + (unsigned char)name[i];
  }
  return hash;
}

}  // extern "C"
