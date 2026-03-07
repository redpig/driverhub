// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_WATCHDOG_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_WATCHDOG_H_

// Watchdog subsystem shim.
//
// Provides:
//   watchdog_init_timeout, watchdog_register_device,
//   watchdog_unregister_device, devm_watchdog_register_device
//   watchdog_set_drvdata, watchdog_get_drvdata
//
// On Fuchsia: could map to fuchsia.hardware.watchdog or stubs.

#include <cstdint>

struct device;

#ifdef __cplusplus
extern "C" {
#endif

#define WATCHDOG_NOWAYOUT   0
#define WDIOF_SETTIMEOUT    0x0080
#define WDIOF_KEEPALIVEPING 0x8000
#define WDIOF_MAGICCLOSE    0x0100

struct watchdog_info {
  uint32_t options;
  uint32_t firmware_version;
  char identity[32];
};

struct watchdog_device;

struct watchdog_ops {
  int (*start)(struct watchdog_device*);
  int (*stop)(struct watchdog_device*);
  int (*ping)(struct watchdog_device*);
  int (*set_timeout)(struct watchdog_device*, unsigned int);
  unsigned int (*get_timeleft)(struct watchdog_device*);
  int (*restart)(struct watchdog_device*, unsigned long, void*);
};

struct watchdog_device {
  int id;
  struct device* parent;
  const struct watchdog_info* info;
  const struct watchdog_ops* ops;
  unsigned int timeout;
  unsigned int min_timeout;
  unsigned int max_timeout;
  unsigned int min_hw_heartbeat_ms;
  unsigned int max_hw_heartbeat_ms;
  unsigned long status;
  void* driver_data;
};

int watchdog_init_timeout(struct watchdog_device* wdd,
                          unsigned int timeout_parm,
                          struct device* dev);
int watchdog_register_device(struct watchdog_device* wdd);
int devm_watchdog_register_device(struct device* dev,
                                   struct watchdog_device* wdd);
void watchdog_unregister_device(struct watchdog_device* wdd);

void watchdog_set_drvdata(struct watchdog_device* wdd, void* data);
void* watchdog_get_drvdata(struct watchdog_device* wdd);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_WATCHDOG_H_
