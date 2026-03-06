// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_SPI_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_SPI_H_

// KMI shims for the Linux SPI subsystem APIs.
//
// Provides: spi_register_driver, spi_unregister_driver,
//           spi_sync, spi_async, spi_write, spi_read
//
// On Fuchsia: maps to fuchsia.hardware.spi/Device FIDL protocol.

#include <stddef.h>
#include <stdint.h>

#include "src/shim/kernel/device.h"

#ifdef __cplusplus
extern "C" {
#endif

// SPI device.
struct spi_device {
  struct device dev;
  uint32_t max_speed_hz;
  uint8_t chip_select;
  uint8_t bits_per_word;
  uint16_t mode;
  int irq;
  void *controller_data;
  const char *modalias;
};

// SPI transfer descriptor.
struct spi_transfer {
  const void *tx_buf;
  void *rx_buf;
  unsigned len;
  uint32_t speed_hz;
  uint8_t bits_per_word;
  unsigned cs_change : 1;
  unsigned tx_nbits : 3;
  unsigned rx_nbits : 3;
};

// SPI message (container for transfers).
struct spi_message {
  struct spi_transfer *transfers;
  unsigned num_transfers;
  void (*complete)(void *context);
  void *context;
  int status;
};

// SPI device ID table.
struct spi_device_id {
  char name[32];
  unsigned long driver_data;
};

struct of_device_id;

// SPI driver structure.
struct spi_driver {
  int (*probe)(struct spi_device *spi);
  void (*remove)(struct spi_device *spi);
  void (*shutdown)(struct spi_device *spi);
  struct {
    const char *name;
    const struct of_device_id *of_match_table;
  } driver;
  const struct spi_device_id *id_table;
};

// Message helpers.
static inline void spi_message_init(struct spi_message *m) {
  m->transfers = 0;
  m->num_transfers = 0;
  m->complete = 0;
  m->context = 0;
  m->status = 0;
}

// Driver registration.
int spi_register_driver(struct spi_driver *driver);
void spi_unregister_driver(struct spi_driver *driver);

#define module_spi_driver(drv) \
  static int __init_##drv(void) { return spi_register_driver(&drv); } \
  static void __exit_##drv(void) { spi_unregister_driver(&drv); } \
  module_init(__init_##drv); \
  module_exit(__exit_##drv)

// Synchronous transfer.
int spi_sync(struct spi_device *spi, struct spi_message *message);

// Asynchronous transfer.
int spi_async(struct spi_device *spi, struct spi_message *message);

// Convenience wrappers.
int spi_write(struct spi_device *spi, const void *buf, size_t len);
int spi_read(struct spi_device *spi, void *buf, size_t len);
int spi_write_then_read(struct spi_device *spi,
                        const void *txbuf, unsigned n_tx,
                        void *rxbuf, unsigned n_rx);

// Data helpers.
static inline void *spi_get_drvdata(struct spi_device *spi) {
  return spi->dev.driver_data;
}

static inline void spi_set_drvdata(struct spi_device *spi, void *data) {
  spi->dev.driver_data = data;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_SPI_H_
