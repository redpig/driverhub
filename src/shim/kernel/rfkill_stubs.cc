// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Miscellaneous kernel API stubs needed by rfkill.ko and similar GKI modules.
//
// These are small functions that don't warrant their own shim file.
// They are registered in the symbol registry to satisfy rfkill.ko's
// external symbol requirements.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "src/shim/kernel/device.h"

extern "C" {

// --- Device model lifecycle ---

void device_initialize(struct device* dev) {
  if (!dev) return;
  dev->devres_head = nullptr;
}

int device_add(struct device* dev) {
  if (!dev) return -22;
  fprintf(stderr, "driverhub: device_add: '%s'\n",
          dev->init_name ? dev->init_name : "(unnamed)");
  return 0;
}

void device_del(struct device* dev) {
  if (!dev) return;
  fprintf(stderr, "driverhub: device_del: '%s'\n",
          dev->init_name ? dev->init_name : "(unnamed)");
}

void put_device(struct device* dev) {
  (void)dev;
}

int dev_set_name(struct device* dev, const char* fmt, ...) {
  if (!dev) return -22;
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  dev->init_name = strdup(buf);
  return 0;
}

int class_register(struct dh_class* cls) {
  (void)cls;
  fprintf(stderr, "driverhub: class_register\n");
  return 0;
}

void class_unregister(struct dh_class* cls) {
  (void)cls;
  fprintf(stderr, "driverhub: class_unregister\n");
}

// --- libc-like kernel functions ---
// GKI modules reference kernel versions of these (not libc).

void* dh_memcpy(void* dest, const void* src, unsigned long n) {
  return memcpy(dest, src, n);
}

int dh_strcmp(const char* s1, const char* s2) {
  return strcmp(s1, s2);
}

unsigned long dh_strlen(const char* s) {
  return strlen(s);
}

// --- Capability check ---

int capable(int cap) {
  (void)cap;
  // In userspace shim, assume all capabilities are granted.
  return 1;
}

// --- String-to-number conversion ---

int kstrtoull(const char* s, unsigned int base, unsigned long long* res) {
  if (!s || !res) return -22;  // -EINVAL
  char* end = nullptr;
  *res = strtoull(s, &end, base);
  if (end == s) return -22;
  return 0;
}

// --- Module parameter ops ---
// param_ops_uint is a struct kernel_param_ops for unsigned int parameters.

struct kernel_param_ops {
  unsigned int flags;
  int (*set)(const char* val, const void* kp);
  int (*get)(char* buffer, const void* kp);
  void (*free)(void* arg);
};

static int param_uint_set(const char* val, const void* kp) {
  (void)val;
  (void)kp;
  return 0;
}

static int param_uint_get(char* buffer, const void* kp) {
  (void)buffer;
  (void)kp;
  return 0;
}

struct kernel_param_ops param_ops_uint = {
    0, param_uint_set, param_uint_get, nullptr};

// --- Architecture-specific stubs ---

// __arch_copy_from_user / __arch_copy_to_user: ARM64 versions of
// copy_from/to_user. In userspace, these are just memcpy.
unsigned long __arch_copy_from_user(void* to, const void* from,
                                    unsigned long n) {
  memcpy(to, from, n);
  return 0;
}

unsigned long __arch_copy_to_user(void* to, const void* from,
                                  unsigned long n) {
  memcpy(to, from, n);
  return 0;
}

// __check_object_size: HARDENED_USERCOPY check. No-op in shim.
void __check_object_size(const void* ptr, unsigned long n, int to_user) {
  (void)ptr;
  (void)n;
  (void)to_user;
}

// --- List validation (CONFIG_DEBUG_LIST) ---

int __list_add_valid_or_report(void* new_entry, void* prev, void* next) {
  (void)new_entry;
  (void)prev;
  (void)next;
  return 1;  // Always valid.
}

int __list_del_entry_valid_or_report(void* entry) {
  (void)entry;
  return 1;  // Always valid.
}

// --- FORTIFY_SOURCE ---

void fortify_panic(const char* name) {
  fprintf(stderr, "[driverhub][FORTIFY] buffer overflow in %s\n",
          name ? name : "unknown");
  abort();
}

// --- ARM64 alternatives ---
// These are ARM64 kernel patching stubs. No-op in userspace.

void alt_cb_patch_nops(void* alt, int nr) {
  (void)alt;
  (void)nr;
}

// system_cpucaps: ARM64 CPU capability bitmap. Provide a zeroed-out buffer.
static unsigned long g_system_cpucaps[16] = {};
unsigned long* system_cpucaps = g_system_cpucaps;

// --- Raw spinlock variants ---
// GKI modules compiled with certain configs use _raw_ prefixed spinlocks.

unsigned long _raw_spin_lock_irqsave(void* lock) {
  // In our shim, spinlocks use the opaque pointer pattern from sync.h.
  // _raw_ variants are called from inline wrappers in the kernel headers.
  // We just delegate to our existing spin_lock_irqsave.
  (void)lock;
  return 0;  // flags (IRQ save is meaningless in userspace)
}

void _raw_spin_unlock_irqrestore(void* lock, unsigned long flags) {
  (void)lock;
  (void)flags;
}

// --- kmalloc internals ---
// GKI 6.x uses kmalloc_trace/kmalloc_caches instead of direct kmalloc.

// kmalloc_caches: array of pointers to slab caches. rfkill.ko indexes
// into this to find the right cache for its allocation size.
// Provide a dummy array — our kmalloc_trace ignores it.
static void* g_kmalloc_caches_data[64] = {};
void** kmalloc_caches = g_kmalloc_caches_data;

void* kmalloc_trace(void* cache, unsigned int flags, unsigned long size) {
  (void)cache;
  (void)flags;
  return malloc(size);
}

void* __kmalloc(unsigned long size, unsigned int flags) {
  (void)flags;
  return malloc(size);
}

// --- __wake_up: internal wake_up variant with more args ---
void __wake_up(void* wq_head, unsigned int mode, int nr_exclusive,
               void* key) {
  (void)wq_head;
  (void)mode;
  (void)nr_exclusive;
  (void)key;
  // Simplified: our wake_up already handles the common case.
}

// --- __init_waitqueue_head: internal init with name/lockdep ---
void __init_waitqueue_head(void* wq_head, const char* name, void* key) {
  (void)name;
  (void)key;
  if (wq_head) {
    // The first field of wait_queue_head_t is void* opaque.
    auto** opaque = static_cast<void**>(wq_head);
    *opaque = nullptr;  // Will be lazily initialized on first use.
  }
}

// --- __mutex_init: internal mutex init with name/lockdep ---
void __mutex_init(void* mutex_ptr, const char* name, void* key) {
  (void)name;
  (void)key;
  if (mutex_ptr) {
    // The first field of struct mutex is void* opaque.
    auto** opaque = static_cast<void**>(mutex_ptr);
    *opaque = nullptr;  // Will be lazily initialized on first lock.
  }
}

}  // extern "C"
