// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Platform driver/device registry.
// In a full Fuchsia integration, platform_driver_register would connect to
// fuchsia.hardware.platform.device/Device FIDL and match against DFv2 bind
// rules derived from DT compatible strings. For now, we keep an in-process
// registry that the bus driver can query.

namespace {

struct RegisteredPlatformDriver {
  struct platform_driver* drv;
};

std::vector<RegisteredPlatformDriver>& driver_registry() {
  static std::vector<RegisteredPlatformDriver> registry;
  return registry;
}

std::vector<struct platform_device*>& device_registry() {
  static std::vector<struct platform_device*> registry;
  return registry;
}

// Try to match a newly added device against registered drivers.
void try_match_device(struct platform_device* pdev) {
  for (auto& entry : driver_registry()) {
    // Match by name.
    if (entry.drv->driver.name && pdev->name &&
        strcmp(entry.drv->driver.name, pdev->name) == 0) {
      fprintf(stderr, "driverhub: platform match %s -> driver %s\n",
              pdev->name, entry.drv->driver.name);
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

// Try to match a newly registered driver against existing devices.
void try_match_driver(struct platform_driver* drv) {
  for (auto* pdev : device_registry()) {
    if (drv->driver.name && pdev->name &&
        strcmp(drv->driver.name, pdev->name) == 0) {
      fprintf(stderr, "driverhub: platform match %s -> driver %s\n",
              pdev->name, drv->driver.name);
      if (drv->probe) {
        int ret = drv->probe(pdev);
        if (ret != 0) {
          fprintf(stderr, "driverhub: probe failed for %s: %d\n",
                  pdev->name, ret);
        }
      }
      return;
    }
  }
}

}  // namespace

extern "C" {

int platform_driver_register(struct platform_driver* drv) {
  fprintf(stderr, "driverhub: platform_driver_register(%s)\n",
          drv->driver.name ? drv->driver.name : "(null)");
  driver_registry().push_back({drv});
  try_match_driver(drv);
  return 0;
}

void platform_driver_unregister(struct platform_driver* drv) {
  fprintf(stderr, "driverhub: platform_driver_unregister(%s)\n",
          drv->driver.name ? drv->driver.name : "(null)");
  auto& registry = driver_registry();
  for (auto it = registry.begin(); it != registry.end(); ++it) {
    if (it->drv == drv) {
      registry.erase(it);
      break;
    }
  }
}

struct platform_device* platform_device_alloc(const char* name, int id) {
  auto* pdev = static_cast<struct platform_device*>(
      calloc(1, sizeof(struct platform_device)));
  if (!pdev) return nullptr;

  pdev->name = strdup(name);
  pdev->id = id;
  pdev->dev.init_name = pdev->name;
  return pdev;
}

int platform_device_add(struct platform_device* pdev) {
  fprintf(stderr, "driverhub: platform_device_add(%s.%d)\n",
          pdev->name ? pdev->name : "(null)", pdev->id);
  device_registry().push_back(pdev);
  try_match_device(pdev);
  return 0;
}

void platform_device_put(struct platform_device* pdev) {
  if (!pdev) return;
  free(const_cast<char*>(pdev->name));
  free(pdev);
}

void platform_device_del(struct platform_device* pdev) {
  fprintf(stderr, "driverhub: platform_device_del(%s.%d)\n",
          pdev->name ? pdev->name : "(null)", pdev->id);
  auto& registry = device_registry();
  for (auto it = registry.begin(); it != registry.end(); ++it) {
    if (*it == pdev) {
      registry.erase(it);
      break;
    }
  }
}

void platform_device_unregister(struct platform_device* pdev) {
  fprintf(stderr, "driverhub: platform_device_unregister(%s.%d)\n",
          pdev->name ? pdev->name : "(null)", pdev->id);
  // Remove from device registry.
  auto& registry = device_registry();
  for (auto it = registry.begin(); it != registry.end(); ++it) {
    if (*it == pdev) {
      registry.erase(it);
      break;
    }
  }
  platform_device_put(pdev);
}

struct resource* platform_get_resource(struct platform_device* pdev,
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

int platform_get_irq(struct platform_device* pdev, unsigned int num) {
  struct resource* r = platform_get_resource(pdev, IORESOURCE_IRQ, num);
  if (!r) return -1;
  return (int)r->start;
}

// __platform_driver_register — the macro-generated form that real .ko modules
// call.  In the standard (non-ABI) build this just delegates to the
// simplified platform_driver_register above.
int __platform_driver_register(struct platform_driver* drv, void* /*owner*/) {
  return platform_driver_register(drv);
}

// device_init_wakeup — no-op in userspace.
int device_init_wakeup(struct device* /*dev*/, int /*enable*/) {
  return 0;
}

}  // extern "C"
