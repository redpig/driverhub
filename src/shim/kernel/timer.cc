// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Timer shim: provides add_timer, del_timer, mod_timer.
// In the userspace shim, timers are simplified — add_timer marks the timer
// active but does not actually schedule a callback. A full implementation
// would use async::PostDelayedTask or a timer thread.

#include <cstdio>

// Avoid pulling in linux/timer.h here to sidestep the POSIX timer_delete
// conflict in C++ mode. Declare the struct directly.
struct timer_list {
  unsigned long expires;
  void (*function)(struct timer_list *);
  unsigned int flags;
  int _active;
};

extern "C" {

void add_timer(struct timer_list *timer) {
  timer->_active = 1;
  fprintf(stderr, "driverhub: timer: add_timer (expires=%lu)\n",
          timer->expires);
}

int del_timer(struct timer_list *timer) {
  int was_active = timer->_active;
  timer->_active = 0;
  return was_active;
}

int mod_timer(struct timer_list *timer, unsigned long expires) {
  int was_active = timer->_active;
  timer->expires = expires;
  timer->_active = 1;
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
