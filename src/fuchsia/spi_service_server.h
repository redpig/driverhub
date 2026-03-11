// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_SPI_SERVICE_SERVER_H_
#define DRIVERHUB_SRC_FUCHSIA_SPI_SERVICE_SERVER_H_

// SPI FIDL service server.
//
// Implements fuchsia.hardware.spi/Device by routing FIDL calls to the
// spi_driver callbacks registered by a loaded .ko module.
//
// Each SpiServiceServer instance serves a single SPI bus. When a downstream
// driver binds to an "spi-<name>" child node and opens its
// fuchsia.hardware.spi/Device service, this server handles TransmitVector()
// and ExchangeVector() calls by invoking the .ko's spi_sync/spi_write/spi_read.

#ifdef __cplusplus
extern "C" {
#endif

struct spi_driver;
struct spi_device;

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __Fuchsia__

#include <fidl/fuchsia.hardware.spi/cpp/fidl.h>
#include <lib/driver/component/cpp/driver_base.h>

namespace driverhub {

// Serves fuchsia.hardware.spi/Device backed by an spi_driver.
class SpiServiceServer
    : public fidl::Server<fuchsia_hardware_spi::Device> {
 public:
  SpiServiceServer(struct spi_driver* drv, struct spi_device* dev);
  ~SpiServiceServer() override;

  // fuchsia.hardware.spi/Device FIDL methods.
  void TransmitVector(TransmitVectorRequest& request,
                      TransmitVectorCompleter::Sync& completer) override;
  void ReceiveVector(ReceiveVectorRequest& request,
                     ReceiveVectorCompleter::Sync& completer) override;
  void ExchangeVector(ExchangeVectorRequest& request,
                      ExchangeVectorCompleter::Sync& completer) override;
  void CanAssertCs(CanAssertCsCompleter::Sync& completer) override;
  void AssertCs(AssertCsCompleter::Sync& completer) override;
  void DeassertCs(DeassertCsCompleter::Sync& completer) override;

 private:
  struct spi_driver* drv_;   // Not owned.
  struct spi_device* dev_;   // Not owned — may be null if no device yet.
};

}  // namespace driverhub

#endif  // __Fuchsia__

#endif  // DRIVERHUB_SRC_FUCHSIA_SPI_SERVICE_SERVER_H_
