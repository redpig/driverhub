// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/workqueue.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <deque>
#include <atomic>

#ifdef __Fuchsia__
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/async/cpp/task.h>
#endif

// Workqueue implementation.
//
// On Fuchsia: uses async::Loop with async::PostTask for native integration
// with Zircon's event loop model. Delayed work uses async::PostDelayedTask.
//
// On POSIX: uses a worker thread with condition variable (for host testing).
//
// The work_struct layout matches the Linux GKI ABI: func is at offset 0x18.
// The pending bit is encoded in work->data bit 0, matching Linux's
// WORK_STRUCT_PENDING_BIT convention.

namespace {

// Check whether work is already pending (bit 0 of data).
inline bool work_is_pending(const struct work_struct* w) {
  return (w->data & WORK_STRUCT_PENDING) != 0;
}

// Mark work as pending (set bit 0).
inline void work_set_pending(struct work_struct* w) {
  w->data |= WORK_STRUCT_PENDING;
}

// Clear pending bit (clear bit 0).
inline void work_clear_pending(struct work_struct* w) {
  w->data &= ~WORK_STRUCT_PENDING;
}

struct WorkqueueImpl {
#ifdef __Fuchsia__
  std::unique_ptr<async::Loop> loop;
#else
  std::thread worker;
  std::condition_variable cv;
  std::deque<struct work_struct*> pending_queue;
#endif
  std::mutex mu;
  std::atomic<bool> shutdown{false};
  const char* name;

#ifdef __Fuchsia__
  WorkqueueImpl() {
    loop = std::make_unique<async::Loop>(&kAsyncLoopConfigNoAttachToCurrentThread);
  }

  void Start() {
    loop->StartThread(name ? name : "driverhub-wq");
  }

  void PostWork(struct work_struct* w) {
    async::PostTask(loop->dispatcher(), [w] {
      if (w && w->func) {
        work_clear_pending(w);
        w->func(w);
      }
    });
  }

  void PostDelayedWork(struct work_struct* w, unsigned long delay_jiffies) {
    // Convert jiffies to nanoseconds. Linux HZ is typically 250 on ARM64.
    // 1 jiffy ≈ 4ms = 4,000,000 ns.
    constexpr uint64_t kNsPerJiffy = 4000000;
    zx::duration delay = zx::nsec(delay_jiffies * kNsPerJiffy);

    async::PostDelayedTask(loop->dispatcher(), [w] {
      if (w && w->func) {
        work_clear_pending(w);
        w->func(w);
      }
    }, delay);
  }

  void Shutdown() {
    shutdown.store(true);
    loop->Shutdown();
  }
#else
  void Run() {
    while (true) {
      struct work_struct* w = nullptr;
      {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [this] {
          return shutdown.load() || !pending_queue.empty();
        });
        if (shutdown.load() && pending_queue.empty()) break;
        w = pending_queue.front();
        pending_queue.pop_front();
      }
      if (w && w->func) {
        work_clear_pending(w);
        w->func(w);
      }
    }
  }

  void Start() {
    worker = std::thread(&WorkqueueImpl::Run, this);
  }

  void PostWork(struct work_struct* w) {
    {
      std::lock_guard<std::mutex> lock(mu);
      pending_queue.push_back(w);
    }
    cv.notify_one();
  }

  void PostDelayedWork(struct work_struct* w, unsigned long /*delay*/) {
    // Simplified: execute immediately on POSIX.
    PostWork(w);
  }

  void Shutdown() {
    shutdown.store(true);
    cv.notify_all();
    if (worker.joinable()) {
      worker.join();
    }
  }
#endif
};

WorkqueueImpl* g_default_wq = nullptr;
std::once_flag g_default_wq_init;

WorkqueueImpl* GetDefaultWq() {
  std::call_once(g_default_wq_init, [] {
    g_default_wq = new WorkqueueImpl();
    g_default_wq->name = "driverhub-wq";
    g_default_wq->Start();
  });
  return g_default_wq;
}

}  // namespace

extern "C" {

int schedule_work(struct work_struct* work) {
  auto* wq = GetDefaultWq();
  return queue_work(reinterpret_cast<struct workqueue_struct*>(wq), work);
}

int cancel_work_sync(struct work_struct* work) {
  int was_pending = work_is_pending(work);
  work_clear_pending(work);
  return was_pending;
}

void flush_work(struct work_struct* work) {
  (void)work;
}

struct workqueue_struct* create_singlethread_workqueue(const char* name) {
  auto* impl = new WorkqueueImpl();
  impl->name = name;
  impl->Start();
  fprintf(stderr, "driverhub: workqueue: created '%s'\n", name);
  return reinterpret_cast<struct workqueue_struct*>(impl);
}

void destroy_workqueue(struct workqueue_struct* wq) {
  auto* impl = reinterpret_cast<WorkqueueImpl*>(wq);
  impl->Shutdown();
  fprintf(stderr, "driverhub: workqueue: destroyed '%s'\n", impl->name);
  delete impl;
}

int queue_work(struct workqueue_struct* wq, struct work_struct* work) {
  auto* impl = reinterpret_cast<WorkqueueImpl*>(wq);
  if (work_is_pending(work)) return 0;  // Already queued.
  work_set_pending(work);
  impl->PostWork(work);
  return 1;
}

int queue_delayed_work(struct workqueue_struct* wq,
                       struct delayed_work* dwork,
                       unsigned long delay) {
  auto* impl = reinterpret_cast<WorkqueueImpl*>(wq);
  if (work_is_pending(&dwork->work)) return 0;
  work_set_pending(&dwork->work);
  impl->PostDelayedWork(&dwork->work, delay);
  return 1;
}

int cancel_delayed_work_sync(struct delayed_work* dwork) {
  return cancel_work_sync(&dwork->work);
}

int cancel_delayed_work(struct delayed_work* dwork) {
  int was_pending = work_is_pending(&dwork->work);
  work_clear_pending(&dwork->work);
  return was_pending;
}

void flush_workqueue(struct workqueue_struct* wq) {
  (void)wq;
}

int queue_work_on(int cpu, struct workqueue_struct* wq,
                  struct work_struct* work) {
  (void)cpu;
  return queue_work(wq, work);
}

int queue_delayed_work_on(int cpu, struct workqueue_struct* wq,
                          struct delayed_work* dwork, unsigned long delay) {
  (void)cpu;
  return queue_delayed_work(wq, dwork, delay);
}

void delayed_work_timer_fn(struct work_struct* work) {
  (void)work;
}

unsigned long round_jiffies_relative(unsigned long j) {
  return j;
}

// Global workqueues — lazily initialized to the default workqueue.
struct workqueue_struct* system_wq = nullptr;
struct workqueue_struct* system_power_efficient_wq = nullptr;

// Ensure global workqueues are initialized before first use.
namespace {
struct GlobalWqInit {
  GlobalWqInit() {
    auto* def = GetDefaultWq();
    system_wq = reinterpret_cast<struct workqueue_struct*>(def);
    system_power_efficient_wq = reinterpret_cast<struct workqueue_struct*>(def);
  }
};
static GlobalWqInit g_global_wq_init;
}  // namespace

}  // extern "C"
