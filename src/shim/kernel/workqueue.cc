// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/workqueue.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <atomic>

// Workqueue implementation using a single worker thread per workqueue.
// schedule_work() uses a default global workqueue.
//
// On Fuchsia, these would map to async::Loop / async::PostTask.

namespace {

struct WorkqueueImpl {
  std::thread worker;
  std::mutex mu;
  std::condition_variable cv;
  std::deque<struct work_struct*> pending;
  std::atomic<bool> shutdown{false};
  const char* name;

  void Run() {
    while (true) {
      struct work_struct* w = nullptr;
      {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [this] { return shutdown.load() || !pending.empty(); });
        if (shutdown.load() && pending.empty()) break;
        w = pending.front();
        pending.pop_front();
      }
      if (w && w->func) {
        w->_pending = 0;
        w->func(w);
      }
    }
  }
};

WorkqueueImpl* g_default_wq = nullptr;
std::once_flag g_default_wq_init;

WorkqueueImpl* GetDefaultWq() {
  std::call_once(g_default_wq_init, [] {
    g_default_wq = new WorkqueueImpl();
    g_default_wq->name = "driverhub-wq";
    g_default_wq->worker = std::thread(&WorkqueueImpl::Run, g_default_wq);
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
  int was_pending = work->_pending;
  work->_pending = 0;
  // In a full implementation, we'd wait for any in-flight execution.
  return was_pending;
}

void flush_work(struct work_struct* work) {
  (void)work;
  // Wait for the work item to complete. Simplified: no-op since our
  // workqueue processes items immediately.
}

struct workqueue_struct* create_singlethread_workqueue(const char* name) {
  auto* impl = new WorkqueueImpl();
  impl->name = name;
  impl->worker = std::thread(&WorkqueueImpl::Run, impl);
  fprintf(stderr, "driverhub: workqueue: created '%s'\n", name);
  return reinterpret_cast<struct workqueue_struct*>(impl);
}

void destroy_workqueue(struct workqueue_struct* wq) {
  auto* impl = reinterpret_cast<WorkqueueImpl*>(wq);
  impl->shutdown.store(true);
  impl->cv.notify_all();
  if (impl->worker.joinable()) {
    impl->worker.join();
  }
  fprintf(stderr, "driverhub: workqueue: destroyed '%s'\n", impl->name);
  delete impl;
}

int queue_work(struct workqueue_struct* wq, struct work_struct* work) {
  auto* impl = reinterpret_cast<WorkqueueImpl*>(wq);
  if (work->_pending) return 0;  // Already queued.
  work->_pending = 1;
  {
    std::lock_guard<std::mutex> lock(impl->mu);
    impl->pending.push_back(work);
  }
  impl->cv.notify_one();
  return 1;
}

int queue_delayed_work(struct workqueue_struct* wq,
                       struct delayed_work* dwork,
                       unsigned long delay) {
  (void)delay;  // Simplified: execute immediately.
  // A full implementation would use a timer or async::PostDelayedTask.
  return queue_work(wq, &dwork->work);
}

int cancel_delayed_work_sync(struct delayed_work* dwork) {
  return cancel_work_sync(&dwork->work);
}

void flush_workqueue(struct workqueue_struct* wq) {
  (void)wq;
  // Simplified: assumes items drain quickly.
}

}  // extern "C"
