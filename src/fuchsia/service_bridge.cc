// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/service_bridge.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/shim/include/linux/gpio/driver.h"
#include "src/shim/subsystem/i2c.h"
#include "src/shim/subsystem/spi.h"
#include "src/shim/subsystem/usb.h"

// Service bridge implementation.
//
// This file implements the glue between Linux subsystem registrations and
// the DFv2 child node creation. When a .ko module registers a GPIO chip,
// I2C driver, SPI driver, or USB driver, the bridge notifies the DFv2
// layer to create child nodes with appropriate service offers.
//
// The bridge is initialized by the DriverHubDriver or KoRunner, which
// provides callbacks for creating and removing DFv2 child nodes.

namespace {

struct BridgeState {
  std::mutex mu;
  dh_bridge_ctx_t ctx = nullptr;
  dh_bridge_add_child_fn add_fn = nullptr;
  dh_bridge_remove_child_fn remove_fn = nullptr;

  // Track registered providers for query API.
  std::vector<struct gpio_chip*> gpio_chips;
  std::vector<struct i2c_driver*> i2c_drivers;
  std::vector<struct spi_driver*> spi_drivers;
  std::vector<struct usb_driver*> usb_drivers;
};

BridgeState& bridge() {
  static BridgeState state;
  return state;
}

}  // namespace

extern "C" {

void dh_bridge_init(dh_bridge_ctx_t ctx,
                    dh_bridge_add_child_fn add_fn,
                    dh_bridge_remove_child_fn remove_fn) {
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);
  b.ctx = ctx;
  b.add_fn = add_fn;
  b.remove_fn = remove_fn;
  fprintf(stderr, "driverhub: service bridge initialized\n");
}

void dh_bridge_teardown(void) {
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);
  b.ctx = nullptr;
  b.add_fn = nullptr;
  b.remove_fn = nullptr;
  b.gpio_chips.clear();
  b.i2c_drivers.clear();
  b.spi_drivers.clear();
  b.usb_drivers.clear();
  fprintf(stderr, "driverhub: service bridge torn down\n");
}

// ---------------------------------------------------------------------------
// GPIO bridge
// ---------------------------------------------------------------------------

void dh_bridge_gpio_chip_added(struct gpio_chip* gc) {
  if (!gc) return;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);

  b.gpio_chips.push_back(gc);

  if (!b.add_fn) {
    fprintf(stderr,
            "driverhub: bridge: gpio chip '%s' registered (no DFv2 bridge)\n",
            gc->label ? gc->label : "(null)");
    return;
  }

  // Create a per-pin child node for each GPIO in the chip.
  // Each child offers fuchsia.hardware.gpio.Service so other drivers
  // (e.g., touchscreen, LED controller) can bind to individual pins.
  for (unsigned i = 0; i < gc->ngpio; i++) {
    char name[64];
    snprintf(name, sizeof(name), "gpio-%d",
             gc->base + static_cast<int>(i));

    int ret = b.add_fn(b.ctx, DH_SERVICE_GPIO, name,
                        gc->base + static_cast<int>(i), gc);
    if (ret != 0) {
      fprintf(stderr,
              "driverhub: bridge: failed to create child for %s: %d\n",
              name, ret);
    }
  }

  fprintf(stderr,
          "driverhub: bridge: gpio chip '%s' → %u DFv2 child nodes created "
          "(pins %d–%d)\n",
          gc->label ? gc->label : "(null)", gc->ngpio,
          gc->base, gc->base + static_cast<int>(gc->ngpio) - 1);
}

void dh_bridge_gpio_chip_removed(struct gpio_chip* gc) {
  if (!gc) return;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);

  // Remove per-pin child nodes.
  if (b.remove_fn) {
    for (unsigned i = 0; i < gc->ngpio; i++) {
      char name[64];
      snprintf(name, sizeof(name), "gpio-%d",
               gc->base + static_cast<int>(i));
      b.remove_fn(b.ctx, DH_SERVICE_GPIO, name, gc);
    }
  }

  // Remove from tracked list.
  for (auto it = b.gpio_chips.begin(); it != b.gpio_chips.end(); ++it) {
    if (*it == gc) {
      b.gpio_chips.erase(it);
      break;
    }
  }

  fprintf(stderr, "driverhub: bridge: gpio chip '%s' removed\n",
          gc->label ? gc->label : "(null)");
}

// ---------------------------------------------------------------------------
// I2C bridge
// ---------------------------------------------------------------------------

void dh_bridge_i2c_driver_added(struct i2c_driver* drv) {
  if (!drv) return;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);

  b.i2c_drivers.push_back(drv);

  if (!b.add_fn) {
    fprintf(stderr,
            "driverhub: bridge: i2c driver '%s' registered (no DFv2 bridge)\n",
            drv->driver.name);
    return;
  }

  // Create a child node offering fuchsia.hardware.i2c.Service.
  // The node name is derived from the driver name.
  char name[128];
  snprintf(name, sizeof(name), "i2c-%s", drv->driver.name);

  int ret = b.add_fn(b.ctx, DH_SERVICE_I2C, name, 0, drv);
  if (ret != 0) {
    fprintf(stderr,
            "driverhub: bridge: failed to create child for %s: %d\n",
            name, ret);
  } else {
    fprintf(stderr,
            "driverhub: bridge: i2c driver '%s' → DFv2 child node created\n",
            drv->driver.name);
  }
}

void dh_bridge_i2c_driver_removed(struct i2c_driver* drv) {
  if (!drv) return;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);

  if (b.remove_fn) {
    char name[128];
    snprintf(name, sizeof(name), "i2c-%s", drv->driver.name);
    b.remove_fn(b.ctx, DH_SERVICE_I2C, name, drv);
  }

  for (auto it = b.i2c_drivers.begin(); it != b.i2c_drivers.end(); ++it) {
    if (*it == drv) {
      b.i2c_drivers.erase(it);
      break;
    }
  }

  fprintf(stderr, "driverhub: bridge: i2c driver '%s' removed\n",
          drv->driver.name);
}

// ---------------------------------------------------------------------------
// SPI bridge
// ---------------------------------------------------------------------------

void dh_bridge_spi_driver_added(struct spi_driver* drv) {
  if (!drv) return;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);

  b.spi_drivers.push_back(drv);

  if (!b.add_fn) {
    fprintf(stderr,
            "driverhub: bridge: spi driver '%s' registered (no DFv2 bridge)\n",
            drv->driver.name);
    return;
  }

  char name[128];
  snprintf(name, sizeof(name), "spi-%s", drv->driver.name);

  int ret = b.add_fn(b.ctx, DH_SERVICE_SPI, name, 0, drv);
  if (ret != 0) {
    fprintf(stderr,
            "driverhub: bridge: failed to create child for %s: %d\n",
            name, ret);
  } else {
    fprintf(stderr,
            "driverhub: bridge: spi driver '%s' → DFv2 child node created\n",
            drv->driver.name);
  }
}

void dh_bridge_spi_driver_removed(struct spi_driver* drv) {
  if (!drv) return;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);

  if (b.remove_fn) {
    char name[128];
    snprintf(name, sizeof(name), "spi-%s", drv->driver.name);
    b.remove_fn(b.ctx, DH_SERVICE_SPI, name, drv);
  }

  for (auto it = b.spi_drivers.begin(); it != b.spi_drivers.end(); ++it) {
    if (*it == drv) {
      b.spi_drivers.erase(it);
      break;
    }
  }

  fprintf(stderr, "driverhub: bridge: spi driver '%s' removed\n",
          drv->driver.name);
}

// ---------------------------------------------------------------------------
// USB bridge
// ---------------------------------------------------------------------------

void dh_bridge_usb_driver_added(struct usb_driver* drv) {
  if (!drv) return;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);

  b.usb_drivers.push_back(drv);

  if (!b.add_fn) {
    fprintf(stderr,
            "driverhub: bridge: usb driver '%s' registered (no DFv2 bridge)\n",
            drv->name);
    return;
  }

  char name[128];
  snprintf(name, sizeof(name), "usb-%s", drv->name);

  int ret = b.add_fn(b.ctx, DH_SERVICE_USB, name, 0, drv);
  if (ret != 0) {
    fprintf(stderr,
            "driverhub: bridge: failed to create child for %s: %d\n",
            name, ret);
  } else {
    fprintf(stderr,
            "driverhub: bridge: usb driver '%s' → DFv2 child node created\n",
            drv->name);
  }
}

void dh_bridge_usb_driver_removed(struct usb_driver* drv) {
  if (!drv) return;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);

  if (b.remove_fn) {
    char name[128];
    snprintf(name, sizeof(name), "usb-%s", drv->name);
    b.remove_fn(b.ctx, DH_SERVICE_USB, name, drv);
  }

  for (auto it = b.usb_drivers.begin(); it != b.usb_drivers.end(); ++it) {
    if (*it == drv) {
      b.usb_drivers.erase(it);
      break;
    }
  }

  fprintf(stderr, "driverhub: bridge: usb driver '%s' removed\n",
          drv->name);
}

// ---------------------------------------------------------------------------
// Query API
// ---------------------------------------------------------------------------

struct gpio_chip* dh_bridge_find_gpio_chip(int base) {
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);
  for (auto* gc : b.gpio_chips) {
    if (gc->base == base) return gc;
    // Also match if the pin falls within this chip's range.
    if (base >= gc->base &&
        base < gc->base + static_cast<int>(gc->ngpio)) {
      return gc;
    }
  }
  return nullptr;
}

struct i2c_driver* dh_bridge_find_i2c_driver(const char* name) {
  if (!name) return nullptr;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);
  for (auto* drv : b.i2c_drivers) {
    if (strcmp(drv->driver.name, name) == 0) return drv;
  }
  return nullptr;
}

struct spi_driver* dh_bridge_find_spi_driver(const char* name) {
  if (!name) return nullptr;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);
  for (auto* drv : b.spi_drivers) {
    if (strcmp(drv->driver.name, name) == 0) return drv;
  }
  return nullptr;
}

struct usb_driver* dh_bridge_find_usb_driver(const char* name) {
  if (!name) return nullptr;
  auto& b = bridge();
  std::lock_guard<std::mutex> lock(b.mu);
  for (auto* drv : b.usb_drivers) {
    if (strcmp(drv->name, name) == 0) return drv;
  }
  return nullptr;
}

}  // extern "C"
