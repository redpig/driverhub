// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/spi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

// SPI subsystem shim.
//
// On Fuchsia, spi_sync would translate to fuchsia.hardware.spi/Device FIDL
// TransferVector calls.

namespace {

std::mutex g_spi_mu;
std::vector<struct spi_driver*> g_spi_drivers;

}  // namespace

extern "C" {

int spi_register_driver(struct spi_driver* driver) {
  std::lock_guard<std::mutex> lock(g_spi_mu);
  g_spi_drivers.push_back(driver);
  fprintf(stderr, "driverhub: spi: registered driver '%s'\n",
          driver->driver.name);
  return 0;
}

void spi_unregister_driver(struct spi_driver* driver) {
  std::lock_guard<std::mutex> lock(g_spi_mu);
  g_spi_drivers.erase(
      std::remove(g_spi_drivers.begin(), g_spi_drivers.end(), driver),
      g_spi_drivers.end());
  fprintf(stderr, "driverhub: spi: unregistered driver '%s'\n",
          driver->driver.name);
}

int spi_sync(struct spi_device* spi, struct spi_message* message) {
  (void)spi;
  // Simulated: mark all transfers as complete.
  message->status = 0;
  fprintf(stderr, "driverhub: spi: sync transfer (simulated)\n");
  return 0;
}

int spi_async(struct spi_device* spi, struct spi_message* message) {
  // Simplified: execute synchronously and call completion.
  int ret = spi_sync(spi, message);
  if (message->complete) {
    message->complete(message->context);
  }
  return ret;
}

int spi_write(struct spi_device* spi, const void* buf, size_t len) {
  (void)spi;
  (void)buf;
  fprintf(stderr, "driverhub: spi: write %zu bytes (simulated)\n", len);
  return 0;
}

int spi_read(struct spi_device* spi, void* buf, size_t len) {
  (void)spi;
  memset(buf, 0, len);
  fprintf(stderr, "driverhub: spi: read %zu bytes (simulated)\n", len);
  return 0;
}

int spi_write_then_read(struct spi_device* spi,
                        const void* txbuf, unsigned n_tx,
                        void* rxbuf, unsigned n_rx) {
  (void)spi;
  (void)txbuf;
  (void)n_tx;
  memset(rxbuf, 0, n_rx);
  fprintf(stderr, "driverhub: spi: write_then_read (tx=%u, rx=%u) (simulated)\n",
          n_tx, n_rx);
  return 0;
}

}  // extern "C"
