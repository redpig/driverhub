// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/sync.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>

// KMI sync shims: map Linux kernel synchronization to C++ std primitives.
// On Fuchsia, spinlocks could use sync_mutex_t and wait queues could use
// zx_futex. For now, std:: primitives suffice for userspace execution.

// Internal backing types stored in the opaque pointer.

struct MutexImpl {
  std::mutex mtx;
};

struct SpinlockImpl {
  std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

struct WaitQueueImpl {
  std::mutex mtx;
  std::condition_variable cv;
};

struct CompletionImpl {
  std::mutex mtx;
  std::condition_variable cv;
  bool done = false;
};

extern "C" {

// --- Mutex ---

void mutex_init(struct mutex* lock) {
  lock->opaque = new MutexImpl();
}

void mutex_destroy(struct mutex* lock) {
  delete static_cast<MutexImpl*>(lock->opaque);
  lock->opaque = nullptr;
}

void mutex_lock(struct mutex* lock) {
  if (!lock->opaque) {
    mutex_init(lock);
  }
  static_cast<MutexImpl*>(lock->opaque)->mtx.lock();
}

void mutex_unlock(struct mutex* lock) {
  if (!lock->opaque) return;
  static_cast<MutexImpl*>(lock->opaque)->mtx.unlock();
}

int mutex_trylock(struct mutex* lock) {
  if (!lock->opaque) {
    mutex_init(lock);
  }
  // Linux returns 1 on success, 0 on failure (opposite of pthread).
  return static_cast<MutexImpl*>(lock->opaque)->mtx.try_lock() ? 1 : 0;
}

// --- Spinlock ---

void spin_lock_init(spinlock_t* lock) {
  lock->opaque = new SpinlockImpl();
}

void spin_lock(spinlock_t* lock) {
  if (!lock->opaque) {
    spin_lock_init(lock);
  }
  auto* impl = static_cast<SpinlockImpl*>(lock->opaque);
  while (impl->flag.test_and_set(std::memory_order_acquire)) {
    // Spin. In a real Fuchsia implementation, we might yield or use
    // arch_spin_relax() equivalent.
  }
}

void spin_unlock(spinlock_t* lock) {
  if (!lock->opaque) return;
  static_cast<SpinlockImpl*>(lock->opaque)->flag.clear(
      std::memory_order_release);
}

void spin_lock_irqsave(spinlock_t* lock, unsigned long flags) {
  (void)flags;  // IRQ disabling is meaningless in userspace.
  spin_lock(lock);
}

void spin_unlock_irqrestore(spinlock_t* lock, unsigned long flags) {
  (void)flags;
  spin_unlock(lock);
}

// --- Wait queue ---

void init_waitqueue_head(wait_queue_head_t* wq) {
  wq->opaque = new WaitQueueImpl();
}

void wake_up(wait_queue_head_t* wq) {
  if (!wq->opaque) return;
  static_cast<WaitQueueImpl*>(wq->opaque)->cv.notify_one();
}

void wake_up_all(wait_queue_head_t* wq) {
  if (!wq->opaque) return;
  static_cast<WaitQueueImpl*>(wq->opaque)->cv.notify_all();
}

// --- Completion ---

void init_completion(struct completion* c) {
  c->opaque = new CompletionImpl();
}

void complete(struct completion* c) {
  if (!c->opaque) {
    init_completion(c);
  }
  auto* impl = static_cast<CompletionImpl*>(c->opaque);
  {
    std::lock_guard<std::mutex> lk(impl->mtx);
    impl->done = true;
  }
  impl->cv.notify_all();
}

void wait_for_completion(struct completion* c) {
  if (!c->opaque) {
    init_completion(c);
  }
  auto* impl = static_cast<CompletionImpl*>(c->opaque);
  std::unique_lock<std::mutex> lk(impl->mtx);
  impl->cv.wait(lk, [impl] { return impl->done; });
}

unsigned long wait_for_completion_timeout(struct completion* c,
                                          unsigned long timeout_jiffies) {
  if (!c->opaque) {
    init_completion(c);
  }
  auto* impl = static_cast<CompletionImpl*>(c->opaque);
  // Approximate: 1 jiffy ≈ 1ms (HZ=1000 on most Android kernels).
  auto timeout = std::chrono::milliseconds(timeout_jiffies);

  std::unique_lock<std::mutex> lk(impl->mtx);
  if (impl->cv.wait_for(lk, timeout, [impl] { return impl->done; })) {
    return timeout_jiffies;  // Remaining jiffies (simplified).
  }
  return 0;  // Timed out.
}

// --- Wait queue internals (used by rfkill.ko) ---

void init_wait_entry(struct wait_queue_entry* wq_entry, int flags) {
  if (!wq_entry) return;
  wq_entry->flags = flags;
  wq_entry->private_data = nullptr;
  wq_entry->func = nullptr;
  wq_entry->entry_opaque = nullptr;
}

long prepare_to_wait_event(wait_queue_head_t* wq_head,
                           struct wait_queue_entry* wq_entry, int state) {
  (void)wq_head;
  (void)wq_entry;
  (void)state;
  // In Linux, this adds the entry to the waitqueue and sets task state.
  // Our simplified version is a no-op; the actual waiting happens in schedule().
  return 0;
}

void finish_wait(wait_queue_head_t* wq_head,
                 struct wait_queue_entry* wq_entry) {
  (void)wq_head;
  (void)wq_entry;
  // Remove entry from waitqueue and reset task state.
}

void schedule(void) {
  // Yield CPU. In Linux this switches to another task.
  // In userspace, briefly yield to allow other threads to run.
  std::this_thread::yield();
}

}  // extern "C"
