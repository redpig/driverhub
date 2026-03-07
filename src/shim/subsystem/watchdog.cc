// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Watchdog subsystem shim.
//
// In userspace, watchdog devices are registered but no hardware timer runs.
// On Fuchsia: maps to fuchsia.hardware.watchdog FIDL.

#include "src/shim/subsystem/watchdog.h"

#include <cstdio>
#include <cstdlib>

namespace {
int g_wdog_next_id = 0;
}

extern "C" {

int watchdog_init_timeout(struct watchdog_device* wdd,
                          unsigned int timeout_parm,
                          struct device* /*dev*/) {
  if (!wdd) return -22;
  if (timeout_parm)
    wdd->timeout = timeout_parm;
  else if (!wdd->timeout)
    wdd->timeout = 60;  // Default 60s.
  return 0;
}

int watchdog_register_device(struct watchdog_device* wdd) {
  if (!wdd) return -22;
  wdd->id = g_wdog_next_id++;
  fprintf(stderr, "driverhub: watchdog: registered '%s' (id=%d, timeout=%us)\n",
          wdd->info ? wdd->info->identity : "unknown",
          wdd->id, wdd->timeout);
  return 0;
}

int devm_watchdog_register_device(struct device* /*dev*/,
                                   struct watchdog_device* wdd) {
  return watchdog_register_device(wdd);
}

void watchdog_unregister_device(struct watchdog_device* wdd) {
  if (!wdd) return;
  fprintf(stderr, "driverhub: watchdog: unregistered (id=%d)\n", wdd->id);
}

void watchdog_set_drvdata(struct watchdog_device* wdd, void* data) {
  if (wdd) wdd->driver_data = data;
}

void* watchdog_get_drvdata(struct watchdog_device* wdd) {
  return wdd ? wdd->driver_data : nullptr;
}

}  // extern "C"
