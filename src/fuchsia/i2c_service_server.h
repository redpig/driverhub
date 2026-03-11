// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_I2C_SERVICE_SERVER_H_
#define DRIVERHUB_SRC_FUCHSIA_I2C_SERVICE_SERVER_H_

// I2C FIDL service server.
//
// Implements fuchsia.hardware.i2c/Device by routing FIDL calls to the
// i2c_driver callbacks registered by a loaded .ko module.
//
// Each I2cServiceServer instance serves a single I2C bus adapter. When
// a downstream driver binds to an "i2c-<name>" child node and opens its
// fuchsia.hardware.i2c/Device service, this server handles Transfer()
// calls by invoking the .ko's i2c_algorithm master_xfer callback.

#ifdef __cplusplus
extern "C" {
#endif

struct i2c_driver;
struct i2c_adapter;

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __Fuchsia__

#include <fidl/fuchsia.hardware.i2c/cpp/fidl.h>
#include <lib/driver/component/cpp/driver_base.h>

namespace driverhub {

// Serves fuchsia.hardware.i2c/Device backed by an i2c_driver's adapter.
class I2cServiceServer
    : public fidl::Server<fuchsia_hardware_i2c::Device> {
 public:
  I2cServiceServer(struct i2c_driver* drv, struct i2c_adapter* adapter);
  ~I2cServiceServer() override;

  // fuchsia.hardware.i2c/Device FIDL method.
  void Transfer(TransferRequest& request,
                TransferCompleter::Sync& completer) override;

  void GetName(GetNameCompleter::Sync& completer) override;

 private:
  struct i2c_driver* drv_;    // Not owned.
  struct i2c_adapter* adapter_;  // Not owned — may be null if no adapter yet.
};

}  // namespace driverhub

#endif  // __Fuchsia__

#endif  // DRIVERHUB_SRC_FUCHSIA_I2C_SERVICE_SERVER_H_
