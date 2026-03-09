// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_LIBC_STUBS_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_LIBC_STUBS_H_

// C library-level symbols that GKI modules expect from the kernel.
//
// The Linux kernel includes its own implementations of memset, memcpy,
// snprintf, etc. When modules are compiled for GKI, they reference these
// as external symbols. We provide them via the C library on the host.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// String/memory operations (kernel built-in equivalents).
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);

// Formatted I/O (kernel versions).
int snprintf(char *buf, size_t size, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int sscanf(const char *buf, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, ...);
int vsscanf(const char *buf, const char *fmt, ...);

// Byte/char helpers.
int tolower(int c);
int toupper(int c);
int isdigit(int c);
int isxdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isspace(int c);
int isprint(int c);

// Simple string conversion.
long simple_strtol(const char *cp, char **endp, unsigned int base);
unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);
long long simple_strtoll(const char *cp, char **endp, unsigned int base);
unsigned long long simple_strtoull(const char *cp, char **endp,
                                    unsigned int base);
int kstrtoint(const char *s, unsigned int base, int *res);
int kstrtouint(const char *s, unsigned int base, unsigned int *res);
int kstrtol(const char *s, unsigned int base, long *res);
int kstrtoul(const char *s, unsigned int base, unsigned long *res);
int kstrtobool(const char *s, int *res);

// strscpy: safe string copy (kernel-specific, not in standard libc).
size_t strscpy(char *dest, const char *src, size_t count);

// kmemdup: allocate and copy.
void *kmemdup(const void *src, size_t len, unsigned int gfp);

// sort/bsearch.
void sort(void *base, size_t num, size_t size,
          int (*cmp)(const void *, const void *),
          void (*swap)(void *, void *, size_t));
void *bsearch(const void *key, const void *base, size_t num, size_t size,
              int (*cmp)(const void *, const void *));

// Hashing.
unsigned int full_name_hash(const void *salt, const char *name,
                             unsigned int len);

// Refcount (used by 29+ modules).
void refcount_warn_saturate(void *r, int old);

// RCU (used by 18+ modules).
void __rcu_read_lock(void);
void __rcu_read_unlock(void);
void synchronize_rcu(void);
void call_rcu(void *head, void (*func)(void *));
void rcu_barrier(void);

// Preemption.
void preempt_schedule_notrace(void);
void preempt_schedule(void);

// BH spinlock variants (used by 15+ modules).
void _raw_spin_lock_bh(void *lock);
void _raw_spin_unlock_bh(void *lock);
void _raw_read_lock(void *lock);
void _raw_read_unlock(void *lock);
void _raw_write_lock_bh(void *lock);
void _raw_write_unlock_bh(void *lock);
void _raw_read_lock_bh(void *lock);
void _raw_read_unlock_bh(void *lock);

// Module reference counting.
int try_module_get(void *module);
void module_put(void *module);

// CPU number (per-cpu variable).
extern unsigned int cpu_number;

// Random.
void get_random_bytes(void *buf, size_t nbytes);

// Misc dev_* print functions.
void _dev_err(const void *dev, const char *fmt, ...);
void _dev_warn(const void *dev, const char *fmt, ...);
void _dev_info(const void *dev, const char *fmt, ...);

// Wait queue additions.
void add_wait_queue(void *wq_head, void *wq_entry);
void remove_wait_queue(void *wq_head, void *wq_entry);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_LIBC_STUBS_H_
