// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_I2C_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_I2C_H_

// KMI shims for the Linux I2C subsystem APIs.
//
// Provides: i2c_add_driver, i2c_del_driver, i2c_transfer,
//           i2c_smbus_read_byte_data, i2c_smbus_write_byte_data,
//           i2c_new_client_device, i2c_unregister_device
//
// On Fuchsia: maps to fuchsia.hardware.i2c/Device FIDL protocol.

#include <stddef.h>
#include <stdint.h>

#include "src/shim/kernel/device.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C message for raw transfers.
struct i2c_msg {
  uint16_t addr;
  uint16_t flags;
  uint16_t len;
  uint8_t *buf;
};

#define I2C_M_RD   0x0001
#define I2C_M_TEN  0x0010

// I2C adapter represents a physical I2C bus.
struct i2c_adapter {
  const char *name;
  int nr;  // Bus number.
  struct device dev;
  void *algo_data;
};

// I2C client represents a device on the bus.
struct i2c_client {
  uint16_t addr;
  char name[48];
  struct i2c_adapter *adapter;
  struct device dev;
  int irq;
};

// I2C device ID table entry.
struct i2c_device_id {
  char name[48];
  unsigned long driver_data;
};

// OF (device tree) match table.
struct of_device_id;

// I2C driver structure.
struct i2c_driver {
  int (*probe)(struct i2c_client *client, const struct i2c_device_id *id);
  void (*remove)(struct i2c_client *client);
  void (*shutdown)(struct i2c_client *client);
  struct {
    const char *name;
    const struct of_device_id *of_match_table;
  } driver;
  const struct i2c_device_id *id_table;
};

// I2C board info for static device declaration.
struct i2c_board_info {
  char type[48];
  uint16_t addr;
  int irq;
  void *platform_data;
};

// Driver registration.
int i2c_add_driver(struct i2c_driver *driver);
void i2c_del_driver(struct i2c_driver *driver);

// Convenience macro matching Linux's module_i2c_driver pattern.
#define module_i2c_driver(drv) \
  static int __init_##drv(void) { return i2c_add_driver(&drv); } \
  static void __exit_##drv(void) { i2c_del_driver(&drv); } \
  module_init(__init_##drv); \
  module_exit(__exit_##drv)

// I2C raw transfer.
int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);

// SMBus convenience functions.
int i2c_smbus_read_byte_data(const struct i2c_client *client, uint8_t command);
int i2c_smbus_write_byte_data(const struct i2c_client *client,
                              uint8_t command, uint8_t value);
int i2c_smbus_read_word_data(const struct i2c_client *client, uint8_t command);
int i2c_smbus_write_word_data(const struct i2c_client *client,
                              uint8_t command, uint16_t value);
int i2c_smbus_read_i2c_block_data(const struct i2c_client *client,
                                  uint8_t command, uint8_t length,
                                  uint8_t *values);

// Device management.
struct i2c_client *i2c_new_client_device(struct i2c_adapter *adap,
                                         struct i2c_board_info const *info);
void i2c_unregister_device(struct i2c_client *client);

// Client data helpers.
static inline void *i2c_get_clientdata(const struct i2c_client *client) {
  return client->dev.driver_data;
}

static inline void i2c_set_clientdata(struct i2c_client *client, void *data) {
  client->dev.driver_data = data;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_I2C_H_
