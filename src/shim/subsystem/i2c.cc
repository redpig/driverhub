// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/i2c.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "src/fuchsia/service_bridge.h"

// I2C subsystem shim.
//
// Maintains an internal registry of I2C drivers and simulated adapters.
// On Fuchsia, i2c_transfer would be backed by the fuchsia.hardware.i2c/Device
// FIDL protocol, translating I2C messages to FIDL read/write transactions.

namespace {

struct I2cDriverEntry {
  struct i2c_driver* driver;
};

std::mutex g_i2c_mu;
std::vector<I2cDriverEntry> g_i2c_drivers;
std::vector<struct i2c_client*> g_i2c_clients;

}  // namespace

extern "C" {

int i2c_add_driver(struct i2c_driver* driver) {
  std::lock_guard<std::mutex> lock(g_i2c_mu);
  g_i2c_drivers.push_back({driver});
  fprintf(stderr, "driverhub: i2c: registered driver '%s'\n",
          driver->driver.name);

  // Notify service bridge to create DFv2 child node with I2C service offer.
  dh_bridge_i2c_driver_added(driver);

  return 0;
}

void i2c_del_driver(struct i2c_driver* driver) {
  // Notify service bridge to remove DFv2 child node.
  dh_bridge_i2c_driver_removed(driver);
  std::lock_guard<std::mutex> lock(g_i2c_mu);
  // Remove all clients bound to this driver.
  for (auto* client : g_i2c_clients) {
    if (driver->remove) {
      driver->remove(client);
    }
  }
  // Remove driver from registry.
  g_i2c_drivers.erase(
      std::remove_if(g_i2c_drivers.begin(), g_i2c_drivers.end(),
                     [driver](const I2cDriverEntry& e) {
                       return e.driver == driver;
                     }),
      g_i2c_drivers.end());
  fprintf(stderr, "driverhub: i2c: unregistered driver '%s'\n",
          driver->driver.name);
}

int i2c_transfer(struct i2c_adapter* adap, struct i2c_msg* msgs, int num) {
  (void)adap;
  // In userspace shim, transfers are no-ops that succeed.
  // On Fuchsia, this would translate to FIDL I2C transactions.
  fprintf(stderr, "driverhub: i2c: transfer %d messages (simulated)\n", num);
  return num;  // Return number of messages transferred.
}

int i2c_smbus_read_byte_data(const struct i2c_client* client,
                             uint8_t command) {
  (void)client;
  (void)command;
  fprintf(stderr, "driverhub: i2c: smbus_read_byte_data(0x%02x) (simulated)\n",
          command);
  return 0;  // Return 0 as default read value.
}

int i2c_smbus_write_byte_data(const struct i2c_client* client,
                              uint8_t command, uint8_t value) {
  (void)client;
  fprintf(stderr,
          "driverhub: i2c: smbus_write_byte_data(0x%02x, 0x%02x) (simulated)\n",
          command, value);
  return 0;
}

int i2c_smbus_read_word_data(const struct i2c_client* client,
                             uint8_t command) {
  (void)client;
  (void)command;
  return 0;
}

int i2c_smbus_write_word_data(const struct i2c_client* client,
                              uint8_t command, uint16_t value) {
  (void)client;
  (void)command;
  (void)value;
  return 0;
}

int i2c_smbus_read_i2c_block_data(const struct i2c_client* client,
                                  uint8_t command, uint8_t length,
                                  uint8_t* values) {
  (void)client;
  (void)command;
  memset(values, 0, length);
  return length;
}

struct i2c_client* i2c_new_client_device(struct i2c_adapter* adap,
                                         struct i2c_board_info const* info) {
  auto* client = static_cast<struct i2c_client*>(
      calloc(1, sizeof(struct i2c_client)));
  client->addr = info->addr;
  strncpy(client->name, info->type, sizeof(client->name) - 1);
  client->adapter = adap;
  client->irq = info->irq;
  client->dev.platform_data = info->platform_data;
  client->dev.init_name = client->name;

  std::lock_guard<std::mutex> lock(g_i2c_mu);
  g_i2c_clients.push_back(client);

  // Try to match against registered drivers.
  for (auto& entry : g_i2c_drivers) {
    auto* drv = entry.driver;
    if (drv->id_table) {
      for (const auto* id = drv->id_table; id->name[0]; id++) {
        if (strcmp(id->name, info->type) == 0) {
          if (drv->probe) {
            drv->probe(client, id);
          }
          break;
        }
      }
    }
  }

  fprintf(stderr, "driverhub: i2c: new client '%s' at 0x%02x\n",
          client->name, client->addr);
  return client;
}

void i2c_unregister_device(struct i2c_client* client) {
  std::lock_guard<std::mutex> lock(g_i2c_mu);
  g_i2c_clients.erase(
      std::remove(g_i2c_clients.begin(), g_i2c_clients.end(), client),
      g_i2c_clients.end());
  fprintf(stderr, "driverhub: i2c: unregistered client '%s'\n", client->name);
  free(client);
}

}  // extern "C"
