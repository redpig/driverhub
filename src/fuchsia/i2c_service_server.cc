// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/i2c_service_server.h"

#include <cstdio>
#include <cstring>

#include "src/shim/subsystem/i2c.h"

#ifdef __Fuchsia__

namespace driverhub {

I2cServiceServer::I2cServiceServer(struct i2c_driver* drv,
                                   struct i2c_adapter* adapter)
    : drv_(drv), adapter_(adapter) {}

I2cServiceServer::~I2cServiceServer() = default;

void I2cServiceServer::Transfer(TransferRequest& request,
                                TransferCompleter::Sync& completer) {
  if (!adapter_) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }

  const auto& transactions = request.transactions();
  if (transactions.empty()) {
    completer.Reply(zx::error(ZX_ERR_INVALID_ARGS));
    return;
  }

  // Build Linux i2c_msg array from FIDL transactions.
  std::vector<struct i2c_msg> msgs;
  // Buffers for read results.
  std::vector<std::vector<uint8_t>> read_bufs;

  for (const auto& txn : transactions) {
    struct i2c_msg msg = {};
    msg.addr = static_cast<uint16_t>(txn.address());

    if (txn.has_write_data()) {
      const auto& write_data = txn.write_data();
      msg.len = static_cast<uint16_t>(write_data.size());
      msg.buf = const_cast<uint8_t*>(write_data.data());
      msg.flags = 0;
      msgs.push_back(msg);
    }

    if (txn.has_read_size()) {
      struct i2c_msg read_msg = {};
      read_msg.addr = static_cast<uint16_t>(txn.address());
      read_msg.flags = I2C_M_RD;
      read_msg.len = static_cast<uint16_t>(txn.read_size());
      read_bufs.emplace_back(txn.read_size(), 0);
      read_msg.buf = read_bufs.back().data();
      msgs.push_back(read_msg);
    }
  }

  int ret = i2c_transfer(adapter_, msgs.data(),
                          static_cast<int>(msgs.size()));
  if (ret < 0) {
    completer.Reply(zx::error(ZX_ERR_IO));
    return;
  }

  // Build read results from read buffers.
  std::vector<fuchsia_hardware_i2c::ReadResult> results;
  for (auto& buf : read_bufs) {
    fuchsia_hardware_i2c::ReadResult result;
    result.data() = std::move(buf);
    results.push_back(std::move(result));
  }

  completer.Reply(zx::ok(std::move(results)));
}

void I2cServiceServer::GetName(GetNameCompleter::Sync& completer) {
  if (drv_ && drv_->driver.name) {
    completer.Reply(zx::ok(std::string(drv_->driver.name)));
  } else {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
  }
}

}  // namespace driverhub

#endif  // __Fuchsia__
