// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Timer shim: provides add_timer, del_timer, mod_timer with actual
// scheduling via a dedicated timer thread.
//
// Timers fire their callback when `jiffies` reaches `expires`.
// On Fuchsia, this would use async::PostDelayedTask on the driver dispatcher.

#include <cstdio>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>
#include <atomic>

// Avoid pulling in linux/timer.h here to sidestep the POSIX timer_delete
// conflict in C++ mode. Declare the struct directly.
struct timer_list {
  unsigned long expires;
  void (*function)(struct timer_list *);
  unsigned int flags;
  int _active;
};

extern "C" volatile unsigned long jiffies;

namespace {

struct TimerCompare {
  bool operator()(const struct timer_list* a,
                  const struct timer_list* b) const {
    if (a->expires != b->expires) return a->expires < b->expires;
    return a < b;  // Tiebreak by address for uniqueness.
  }
};

std::mutex g_timer_mu;
std::condition_variable g_timer_cv;
std::set<struct timer_list*, TimerCompare> g_active_timers;
std::atomic<bool> g_timer_shutdown{false};
std::once_flag g_timer_thread_init;
std::thread g_timer_thread;

void TimerThreadFunc() {
  while (!g_timer_shutdown.load()) {
    std::unique_lock<std::mutex> lock(g_timer_mu);

    if (g_active_timers.empty()) {
      g_timer_cv.wait_for(lock, std::chrono::milliseconds(10));
      continue;
    }

    // Check if the earliest timer has expired.
    auto* timer = *g_active_timers.begin();
    unsigned long now = jiffies;
    if (timer->expires <= now) {
      g_active_timers.erase(g_active_timers.begin());
      timer->_active = 0;
      auto func = timer->function;
      lock.unlock();

      // Fire the callback outside the lock.
      if (func) {
        func(timer);
      }
    } else {
      // Sleep until the next timer fires (1ms granularity).
      g_timer_cv.wait_for(lock, std::chrono::milliseconds(1));
    }
  }
}

void EnsureTimerThread() {
  std::call_once(g_timer_thread_init, [] {
    g_timer_thread = std::thread(TimerThreadFunc);
    g_timer_thread.detach();
  });
}

}  // namespace

extern "C" {

void add_timer(struct timer_list *timer) {
  EnsureTimerThread();
  std::lock_guard<std::mutex> lock(g_timer_mu);
  timer->_active = 1;
  g_active_timers.insert(timer);
  g_timer_cv.notify_one();
}

int del_timer(struct timer_list *timer) {
  std::lock_guard<std::mutex> lock(g_timer_mu);
  int was_active = timer->_active;
  if (was_active) {
    g_active_timers.erase(timer);
    timer->_active = 0;
  }
  return was_active;
}

int mod_timer(struct timer_list *timer, unsigned long expires) {
  EnsureTimerThread();
  std::lock_guard<std::mutex> lock(g_timer_mu);
  int was_active = timer->_active;
  if (was_active) {
    g_active_timers.erase(timer);
  }
  timer->expires = expires;
  timer->_active = 1;
  g_active_timers.insert(timer);
  g_timer_cv.notify_one();
  return was_active;
}

// init_timer_key — called by the timer_setup() macro in real .ko modules.
void init_timer_key(struct timer_list *timer, void (*func)(struct timer_list *),
                    unsigned int flags, const char * /*name*/,
                    void * /*key*/) {
  timer->function = func;
  timer->flags = flags;
  timer->_active = 0;
  timer->expires = 0;
}

}  // extern "C"
