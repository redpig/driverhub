// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_GPIO_SERVICE_SERVER_H_
#define DRIVERHUB_SRC_FUCHSIA_GPIO_SERVICE_SERVER_H_

// GPIO FIDL service server.
//
// Implements fuchsia.hardware.gpio/Gpio by routing FIDL calls to the
// gpio_chip callbacks registered by a loaded .ko module.
//
// Each GpioServiceServer instance serves a single GPIO pin. When a
// downstream driver (e.g., touchscreen, LED controller) binds to a
// "gpio-N" child node and opens its fuchsia.hardware.gpio/Gpio service,
// this server handles the calls:
//
//   ConfigIn()        → gc->direction_input(gc, offset)
//   ConfigOut(value)  → gc->direction_output(gc, offset, value)
//   Read()            → gc->get(gc, offset)
//   Write(value)      → gc->set(gc, offset, value)
//   GetInterrupt()    → gc->to_irq(gc, offset) + interrupt setup
//
// This is the key component that makes GKI GPIO controller modules
// appear as native Fuchsia GPIO providers for composite node binding.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

struct gpio_chip;

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __Fuchsia__

#include <fidl/fuchsia.hardware.gpio/cpp/fidl.h>
#include <lib/driver/component/cpp/driver_base.h>

namespace driverhub {

// Serves fuchsia.hardware.gpio/Gpio for a single pin backed by a gpio_chip.
class GpioServiceServer
    : public fidl::Server<fuchsia_hardware_gpio::Gpio> {
 public:
  GpioServiceServer(struct gpio_chip* chip, unsigned int offset);
  ~GpioServiceServer() override;

  // fuchsia.hardware.gpio/Gpio FIDL methods.
  void ConfigIn(ConfigInRequest& request,
                ConfigInCompleter::Sync& completer) override;
  void ConfigOut(ConfigOutRequest& request,
                 ConfigOutCompleter::Sync& completer) override;
  void Read(ReadCompleter::Sync& completer) override;
  void Write(WriteRequest& request,
             WriteCompleter::Sync& completer) override;
  void SetBufferMode(SetBufferModeRequest& request,
                     SetBufferModeCompleter::Sync& completer) override;
  void GetInterrupt(GetInterruptRequest& request,
                    GetInterruptCompleter::Sync& completer) override;
  void ReleaseInterrupt(
      ReleaseInterruptCompleter::Sync& completer) override;
  void SetDriveStrength(SetDriveStrengthRequest& request,
                        SetDriveStrengthCompleter::Sync& completer) override;
  void GetDriveStrength(
      GetDriveStrengthCompleter::Sync& completer) override;
  void GetPin(GetPinCompleter::Sync& completer) override;

 private:
  struct gpio_chip* chip_;  // Not owned.
  unsigned int offset_;     // Pin offset within the chip.
};

}  // namespace driverhub

#endif  // __Fuchsia__

#endif  // DRIVERHUB_SRC_FUCHSIA_GPIO_SERVICE_SERVER_H_
