// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ABI-compatible timer shim for loading real GKI .ko modules.

#include <cstdio>
#include <cstring>

extern "C" {

#include "src/shim/include/linux/abi/structs.h"

// The real kernel's timer_setup() macro calls init_timer_key().
// This initializes the timer_list struct fields.
void init_timer_key(struct timer_list *timer, void (*func)(struct timer_list *),
                    unsigned int flags, const char *name, void *key) {
  (void)name;
  (void)key;
  memset(timer->_pad0, 0, sizeof(timer->_pad0));  // list_head init
  timer->expires = 0;
  timer->function = func;
  timer->flags = flags;
  fprintf(stderr, "driverhub: timer: init_timer_key (func=%p)\n",
          reinterpret_cast<void *>(func));
}

void add_timer(struct timer_list *timer) {
  fprintf(stderr, "driverhub: timer: add_timer (expires=%lu)\n",
          timer->expires);
}

int del_timer(struct timer_list *timer) {
  (void)timer;
  return 0;
}

int mod_timer(struct timer_list *timer, unsigned long expires) {
  timer->expires = expires;
  return 0;
}

}  // extern "C"
