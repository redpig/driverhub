// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ABI-compatible platform device/driver shim for loading real GKI .ko modules.
// Uses struct layouts that match the Linux 5.15 x86_64 kernel ABI.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {

#include "src/shim/include/linux/abi/structs.h"

namespace {

struct RegisteredPlatformDriver {
  struct platform_driver *drv;
};

std::vector<RegisteredPlatformDriver> &driver_registry() {
  static std::vector<RegisteredPlatformDriver> registry;
  return registry;
}

std::vector<struct platform_device *> &device_registry() {
  static std::vector<struct platform_device *> registry;
  return registry;
}

const char *pdrv_name(struct platform_driver *drv) {
  return drv->driver_name ? drv->driver_name : "(null)";
}

void try_match_device(struct platform_device *pdev) {
  for (auto &entry : driver_registry()) {
    if (entry.drv->driver_name && pdev->name &&
        strcmp(entry.drv->driver_name, pdev->name) == 0) {
      fprintf(stderr, "driverhub: platform match %s -> driver %s\n",
              pdev->name, pdrv_name(entry.drv));
      if (entry.drv->probe) {
        int ret = entry.drv->probe(pdev);
        if (ret != 0) {
          fprintf(stderr, "driverhub: probe failed for %s: %d\n",
                  pdev->name, ret);
        }
      }
      return;
    }
  }
}

void try_match_driver(struct platform_driver *drv) {
  for (auto *pdev : device_registry()) {
    if (drv->driver_name && pdev->name &&
        strcmp(drv->driver_name, pdev->name) == 0) {
      fprintf(stderr, "driverhub: platform match %s -> driver %s\n",
              pdev->name, pdrv_name(drv));
      if (drv->probe) {
        int ret = drv->probe(pdev);
        if (ret != 0) {
          fprintf(stderr, "driverhub: probe failed for %s: %d\n",
                  pdev->name, ret);
        }
      }
      // Don't return — match all existing devices.
    }
  }
}

}  // namespace

// The real kernel's platform_driver_register is a macro that calls
// __platform_driver_register with the module owner. The .ko will
// reference __platform_driver_register as an undefined symbol.
int __platform_driver_register(struct platform_driver *drv, void *owner) {
  (void)owner;
  fprintf(stderr, "driverhub: __platform_driver_register(%s)\n",
          pdrv_name(drv));
  driver_registry().push_back({drv});
  try_match_driver(drv);
  return 0;
}

// Also provide the non-underscore version for our shim-compiled modules.
int platform_driver_register(struct platform_driver *drv) {
  return __platform_driver_register(drv, nullptr);
}

void platform_driver_unregister(struct platform_driver *drv) {
  fprintf(stderr, "driverhub: platform_driver_unregister(%s)\n",
          pdrv_name(drv));
  auto &registry = driver_registry();
  for (auto it = registry.begin(); it != registry.end(); ++it) {
    if (it->drv == drv) {
      registry.erase(it);
      break;
    }
  }
}

struct platform_device *platform_device_alloc(const char *name, int id) {
  auto *pdev = static_cast<struct platform_device *>(
      calloc(1, sizeof(struct platform_device)));
  if (!pdev) return nullptr;

  pdev->name = strdup(name);
  pdev->id = id;
  pdev->dev.init_name = pdev->name;
  return pdev;
}

int platform_device_add(struct platform_device *pdev) {
  fprintf(stderr, "driverhub: platform_device_add(%s.%d)\n",
          pdev->name ? pdev->name : "(null)", pdev->id);
  device_registry().push_back(pdev);
  try_match_device(pdev);
  return 0;
}

void platform_device_del(struct platform_device *pdev) {
  fprintf(stderr, "driverhub: platform_device_del(%s.%d)\n",
          pdev->name ? pdev->name : "(null)", pdev->id);
  auto &registry = device_registry();
  for (auto it = registry.begin(); it != registry.end(); ++it) {
    if (*it == pdev) {
      registry.erase(it);
      break;
    }
  }
}

void platform_device_put(struct platform_device *pdev) {
  if (!pdev) return;
  free(const_cast<char *>(pdev->name));
  free(pdev);
}

void platform_device_unregister(struct platform_device *pdev) {
  if (!pdev) return;
  fprintf(stderr, "driverhub: platform_device_unregister(%s.%d)\n",
          pdev->name ? pdev->name : "(null)", pdev->id);
  platform_device_del(pdev);
  platform_device_put(pdev);
}

struct resource *platform_get_resource(struct platform_device *pdev,
                                       unsigned int type, unsigned int num) {
  unsigned int count = 0;
  for (unsigned int i = 0; i < pdev->num_resources; i++) {
    if ((pdev->resource[i].flags & type) == type) {
      if (count == num) {
        return &pdev->resource[i];
      }
      count++;
    }
  }
  return nullptr;
}

int platform_get_irq(struct platform_device *pdev, unsigned int num) {
  struct resource *r = platform_get_resource(pdev, 0x00000400, num);
  if (!r) return -1;
  return (int)r->start;
}

// device_init_wakeup — no-op in userspace, but real .ko expects a symbol.
int device_init_wakeup(struct device *dev, int enable) {
  (void)dev;
  (void)enable;
  return 0;
}

// devm_kmalloc / devm_kzalloc — device-managed allocation.
void *devm_kmalloc(struct device *dev, size_t size, unsigned int flags) {
  (void)dev;
  (void)flags;
  return malloc(size);
}

void *devm_kzalloc(struct device *dev, size_t size, unsigned int flags) {
  (void)dev;
  (void)flags;
  return calloc(1, size);
}

}  // extern "C"
