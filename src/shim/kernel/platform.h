// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_PLATFORM_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_PLATFORM_H_

// KMI shims for the Linux platform device/driver model.
//
// Provides: platform_driver, platform_device, platform_driver_register,
//           platform_driver_unregister, platform_device_alloc,
//           platform_device_add, platform_device_put,
//           platform_device_unregister, platform_get_resource,
//           platform_get_irq, module_platform_driver
//
// On Fuchsia, platform_driver_register would bind to
// fuchsia.hardware.platform.device/Device FIDL. For now, we maintain an
// internal registry that the bus driver queries when matching DT nodes.

#include "src/shim/kernel/device.h"

#ifdef __cplusplus
extern "C" {
#endif

struct platform_device {
  const char* name;
  int id;
  struct device dev;
  unsigned int num_resources;
  struct resource* resource;
};

struct of_device_id {
  char compatible[128];
  const void* data;
};

struct platform_driver {
  int (*probe)(struct platform_device*);
  int (*remove)(struct platform_device*);
  void (*shutdown)(struct platform_device*);
  struct {
    const char* name;
    const struct of_device_id* of_match_table;
  } driver;
};

// platform_device_info — used by platform_device_register_full().
struct platform_device_info {
  const char* name;
  int id;
  const void* data;
  unsigned long size_data;
  unsigned int num_res;
  const struct resource* res;
};

int platform_driver_register(struct platform_driver* drv);
void platform_driver_unregister(struct platform_driver* drv);

struct platform_device* platform_device_alloc(const char* name, int id);
int platform_device_add(struct platform_device* pdev);
void platform_device_del(struct platform_device* pdev);
void platform_device_put(struct platform_device* pdev);
void platform_device_unregister(struct platform_device* pdev);

struct resource* platform_get_resource(struct platform_device* pdev,
                                       unsigned int type, unsigned int num);
int platform_get_irq(struct platform_device* pdev, unsigned int num);

struct platform_device* platform_device_register_full(
    const struct platform_device_info* pdevinfo);

static inline void* platform_get_drvdata(const struct platform_device* pdev) {
  return dev_get_drvdata(&pdev->dev);
}

static inline void platform_set_drvdata(struct platform_device* pdev,
                                        void* data) {
  dev_set_drvdata(&pdev->dev, data);
}

// Convenience macro matching Linux's module_platform_driver pattern.
// Generates module_init/module_exit that register/unregister the driver.
#define module_platform_driver(drv) \
  static int __init_##drv(void) { return platform_driver_register(&drv); } \
  static void __exit_##drv(void) { platform_driver_unregister(&drv); } \
  module_init(__init_##drv); \
  module_exit(__exit_##drv)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_PLATFORM_H_
