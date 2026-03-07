// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/gpio_service_server.h"

#include <cstdio>

#include "src/shim/include/linux/gpio/driver.h"

#ifdef __Fuchsia__

namespace driverhub {

GpioServiceServer::GpioServiceServer(struct gpio_chip* chip,
                                     unsigned int offset)
    : chip_(chip), offset_(offset) {}

GpioServiceServer::~GpioServiceServer() = default;

void GpioServiceServer::ConfigIn(ConfigInRequest& request,
                                 ConfigInCompleter::Sync& completer) {
  if (!chip_ || !chip_->direction_input) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }
  int ret = chip_->direction_input(chip_, offset_);
  if (ret != 0) {
    completer.Reply(zx::error(ZX_ERR_IO));
    return;
  }
  completer.Reply(zx::ok());
}

void GpioServiceServer::ConfigOut(ConfigOutRequest& request,
                                  ConfigOutCompleter::Sync& completer) {
  if (!chip_ || !chip_->direction_output) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }
  int ret = chip_->direction_output(chip_, offset_,
                                     request.initial_value());
  if (ret != 0) {
    completer.Reply(zx::error(ZX_ERR_IO));
    return;
  }
  completer.Reply(zx::ok());
}

void GpioServiceServer::Read(ReadCompleter::Sync& completer) {
  if (!chip_ || !chip_->get) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }
  int val = chip_->get(chip_, offset_);
  completer.Reply(zx::ok(static_cast<uint8_t>(val)));
}

void GpioServiceServer::Write(WriteRequest& request,
                               WriteCompleter::Sync& completer) {
  if (!chip_ || !chip_->set) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }
  chip_->set(chip_, offset_, request.value());
  completer.Reply(zx::ok());
}

void GpioServiceServer::SetBufferMode(
    SetBufferModeRequest& request,
    SetBufferModeCompleter::Sync& completer) {
  // Buffer mode is not typically supported by Linux GPIO controllers.
  completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
}

void GpioServiceServer::GetInterrupt(
    GetInterruptRequest& request,
    GetInterruptCompleter::Sync& completer) {
  if (!chip_ || !chip_->to_irq) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }
  int irq = chip_->to_irq(chip_, offset_);
  if (irq < 0) {
    completer.Reply(zx::error(ZX_ERR_NOT_FOUND));
    return;
  }
  // In the full implementation, this would create a zx::interrupt from
  // the IRQ number and return it. For now, return NOT_SUPPORTED since
  // creating a real interrupt object requires platform device cooperation.
  completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
}

void GpioServiceServer::ReleaseInterrupt(
    ReleaseInterruptCompleter::Sync& completer) {
  completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
}

void GpioServiceServer::SetDriveStrength(
    SetDriveStrengthRequest& request,
    SetDriveStrengthCompleter::Sync& completer) {
  // Drive strength not exposed by standard Linux GPIO API.
  completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
}

void GpioServiceServer::GetDriveStrength(
    GetDriveStrengthCompleter::Sync& completer) {
  completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
}

void GpioServiceServer::GetPin(GetPinCompleter::Sync& completer) {
  if (!chip_) {
    completer.Reply(zx::error(ZX_ERR_BAD_STATE));
    return;
  }
  completer.Reply(
      zx::ok(static_cast<uint32_t>(chip_->base +
                                    static_cast<int>(offset_))));
}

}  // namespace driverhub

#endif  // __Fuchsia__
