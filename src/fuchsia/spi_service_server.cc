// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/spi_service_server.h"

#include <cstdio>

#include "src/shim/subsystem/spi.h"

#ifdef __Fuchsia__

namespace driverhub {

SpiServiceServer::SpiServiceServer(struct spi_driver* drv,
                                   struct spi_device* dev)
    : drv_(drv), dev_(dev) {}

SpiServiceServer::~SpiServiceServer() = default;

void SpiServiceServer::TransmitVector(
    TransmitVectorRequest& request,
    TransmitVectorCompleter::Sync& completer) {
  if (!dev_) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }

  const auto& data = request.data();
  int ret = spi_write(dev_, data.data(), data.size());
  if (ret != 0) {
    completer.Reply(zx::error(ZX_ERR_IO));
    return;
  }
  completer.Reply(zx::ok());
}

void SpiServiceServer::ReceiveVector(
    ReceiveVectorRequest& request,
    ReceiveVectorCompleter::Sync& completer) {
  if (!dev_) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }

  uint32_t size = request.size();
  std::vector<uint8_t> buf(size, 0);
  int ret = spi_read(dev_, buf.data(), size);
  if (ret != 0) {
    completer.Reply(zx::error(ZX_ERR_IO));
    return;
  }
  completer.Reply(zx::ok(std::move(buf)));
}

void SpiServiceServer::ExchangeVector(
    ExchangeVectorRequest& request,
    ExchangeVectorCompleter::Sync& completer) {
  if (!dev_) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }

  const auto& tx_data = request.txdata();
  std::vector<uint8_t> rx_buf(tx_data.size(), 0);

  int ret = spi_write_then_read(dev_,
                                 tx_data.data(),
                                 static_cast<unsigned>(tx_data.size()),
                                 rx_buf.data(),
                                 static_cast<unsigned>(rx_buf.size()));
  if (ret != 0) {
    completer.Reply(zx::error(ZX_ERR_IO));
    return;
  }
  completer.Reply(zx::ok(std::move(rx_buf)));
}

void SpiServiceServer::CanAssertCs(
    CanAssertCsCompleter::Sync& completer) {
  // Linux SPI drivers manage CS internally.
  completer.Reply(false);
}

void SpiServiceServer::AssertCs(AssertCsCompleter::Sync& completer) {
  completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
}

void SpiServiceServer::DeassertCs(DeassertCsCompleter::Sync& completer) {
  completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
}

}  // namespace driverhub

#endif  // __Fuchsia__
