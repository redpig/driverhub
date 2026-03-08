// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_SYNC_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_SYNC_H_

// KMI shims for Linux kernel synchronization primitives.
//
// Provides: mutex, spinlock, wait_queue, completion
//
// mutex → std::mutex / sync_mutex_t
// spinlock → userspace spinlock (std::atomic)
// wait_queue → condition variable / zx_futex
// completion → one-shot event

#ifdef __cplusplus
extern "C" {
#endif

// --- Mutex ---

struct mutex {
  void* opaque;  // Backing implementation
};

#define DEFINE_MUTEX(name) struct mutex name = {.opaque = NULL}

void mutex_init(struct mutex* lock);
void mutex_destroy(struct mutex* lock);
void mutex_lock(struct mutex* lock);
void mutex_unlock(struct mutex* lock);
int mutex_trylock(struct mutex* lock);

// --- Spinlock ---

typedef struct {
  void* opaque;
} spinlock_t;

#define DEFINE_SPINLOCK(name) spinlock_t name = {.opaque = NULL}

void spin_lock_init(spinlock_t* lock);
void spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock);
void spin_lock_irqsave(spinlock_t* lock, unsigned long flags);
void spin_unlock_irqrestore(spinlock_t* lock, unsigned long flags);

// --- Wait queue ---

struct wait_queue_head {
  void* opaque;
};

typedef struct wait_queue_head wait_queue_head_t;

// Wait queue entry (used by prepare_to_wait_event / finish_wait).
struct wait_queue_entry {
  unsigned int flags;
  void* private_data;
  void* func;  // wake function
  void* entry_opaque;
};

void init_waitqueue_head(wait_queue_head_t* wq);
void wake_up(wait_queue_head_t* wq);
void wake_up_all(wait_queue_head_t* wq);

// Internal wait queue helpers (used by rfkill.ko).
void init_wait_entry(struct wait_queue_entry* wq_entry, int flags);
long prepare_to_wait_event(wait_queue_head_t* wq_head,
                           struct wait_queue_entry* wq_entry, int state);
void finish_wait(wait_queue_head_t* wq_head,
                 struct wait_queue_entry* wq_entry);
void schedule(void);

// --- Completion ---

struct completion {
  void* opaque;
};

void init_completion(struct completion* c);
void complete(struct completion* c);
void wait_for_completion(struct completion* c);
unsigned long wait_for_completion_timeout(struct completion* c,
                                          unsigned long timeout_jiffies);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_SYNC_H_
