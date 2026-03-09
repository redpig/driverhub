// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_WORKQUEUE_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_WORKQUEUE_H_

// KMI shims for Linux kernel workqueue APIs.
//
// IMPORTANT: struct work_struct and struct delayed_work must match the
// Linux GKI ABI exactly. Pre-built .ko modules write to these structs at
// compiled-in offsets. If our layout doesn't match, the workqueue thread
// will read garbage for the func pointer and crash.
//
// Linux ABI (arm64, kernel 6.x):
//   work_struct: 32 bytes
//     data      @ 0x00  (8 bytes, atomic_long_t)
//     entry     @ 0x08  (16 bytes, struct list_head: next + prev)
//     func      @ 0x18  (8 bytes, work_func_t)
//
//   delayed_work: 120 bytes
//     work      @ 0x00  (32 bytes)
//     timer     @ 0x20  (40 bytes, struct timer_list)
//     wq        @ 0x48  (8 bytes)
//     cpu       @ 0x50  (4 bytes)

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- work_struct (32 bytes, ABI-compatible) ---

// Bit 0 of data encodes pending state (WORK_STRUCT_PENDING_BIT).
#define WORK_STRUCT_PENDING_BIT  0
#define WORK_STRUCT_PENDING      (1UL << WORK_STRUCT_PENDING_BIT)
// Bits used for pool/flags in Linux. We set NO_POOL to signal "not queued".
#define WORK_STRUCT_NO_POOL      ((unsigned long)0xFFFFFFE0UL)

struct list_head {
  struct list_head *next;
  struct list_head *prev;
};

struct work_struct {
  unsigned long data;                   // 0x00: flags + pool ID
  struct list_head entry;               // 0x08: linked-list entry
  void (*func)(struct work_struct *);   // 0x18: work function
};

// Verify ABI layout at compile time.
#ifdef __cplusplus
static_assert(sizeof(struct work_struct) == 32, "work_struct size mismatch");
static_assert(__builtin_offsetof(struct work_struct, data) == 0,
              "work_struct.data offset");
static_assert(__builtin_offsetof(struct work_struct, entry) == 8,
              "work_struct.entry offset");
static_assert(__builtin_offsetof(struct work_struct, func) == 24,
              "work_struct.func offset");
#endif

// --- delayed_work (ABI-compatible) ---
// Embeds work_struct + timer_list (from abi/structs.h, 40 bytes).

struct timer_list;  // Forward declaration; defined in abi/structs.h.

struct delayed_work {
  struct work_struct work;              // 0x00 (32 bytes)
  char _timer_pad[40];                  // 0x20: struct timer_list (40 bytes)
  void *wq;                            // 0x48: struct workqueue_struct *
  int cpu;                              // 0x50
  int _pad;                             // 0x54: alignment
};

#ifdef __cplusplus
static_assert(sizeof(struct delayed_work) == 88, "delayed_work size mismatch");
static_assert(__builtin_offsetof(struct delayed_work, work) == 0,
              "delayed_work.work offset");
static_assert(__builtin_offsetof(struct delayed_work, wq) == 72,
              "delayed_work.wq offset");
static_assert(__builtin_offsetof(struct delayed_work, cpu) == 80,
              "delayed_work.cpu offset");
#endif

struct workqueue_struct;

// INIT_WORK: Initialize a work_struct. Module code uses compiled-in inlines
// but our own code (shim layer, demos) can use these macros.
#define INIT_WORK(w, f) do {                       \
  (w)->data = WORK_STRUCT_NO_POOL;                 \
  (w)->entry.next = &(w)->entry;                   \
  (w)->entry.prev = &(w)->entry;                   \
  (w)->func = (f);                                 \
} while (0)

#define INIT_DELAYED_WORK(dw, f) do {              \
  INIT_WORK(&(dw)->work, (f));                     \
  __builtin_memset((dw)->_timer_pad, 0, 40);       \
  (dw)->wq = 0;                                    \
  (dw)->cpu = 0;                                   \
} while (0)

int schedule_work(struct work_struct *work);
int cancel_work_sync(struct work_struct *work);
void flush_work(struct work_struct *work);

struct workqueue_struct *create_singlethread_workqueue(const char *name);
void destroy_workqueue(struct workqueue_struct *wq);

int queue_work(struct workqueue_struct *wq, struct work_struct *work);
int queue_delayed_work(struct workqueue_struct *wq,
                       struct delayed_work *dwork,
                       unsigned long delay);
int cancel_delayed_work_sync(struct delayed_work *dwork);
int cancel_delayed_work(struct delayed_work *dwork);

// Flush all pending work on a workqueue.
void flush_workqueue(struct workqueue_struct *wq);

// queue_work_on / queue_delayed_work_on — CPU-specific variants.
int queue_work_on(int cpu, struct workqueue_struct *wq,
                  struct work_struct *work);
int queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
                          struct delayed_work *dwork, unsigned long delay);

// Timer function for delayed work (used as callback).
void delayed_work_timer_fn(struct work_struct *work);

// Round jiffies to next full second boundary for power efficiency.
unsigned long round_jiffies_relative(unsigned long j);

// Global workqueue pointers (system_wq, system_power_efficient_wq).
extern struct workqueue_struct *system_wq;
extern struct workqueue_struct *system_power_efficient_wq;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_WORKQUEUE_H_
