// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_GPIO_LED_DRIVER_H_
#define DRIVERHUB_SRC_FUCHSIA_GPIO_LED_DRIVER_H_

// Fuchsia-native DFv2 GPIO LED driver.
//
// This driver demonstrates a pure-Fuchsia DFv2 driver that consumes GPIO
// pins provided by a .ko module loaded through DriverHub. It proves that
// native Fuchsia drivers can composite-bind to services backed by Linux
// kernel modules.
//
// Topology:
//
//   platform_bus
//     └── driverhub (bus driver)
//           ├── gpio_controller_module (.ko)
//           │     ├── gpio-0 (offers fuchsia.hardware.gpio.Service)
//           │     ├── gpio-1
//           │     ├── ...
//           │     └── gpio-31
//           └── gpio-led-driver (this driver, native DFv2)
//                 binds to: gpio-5 (LED pin)
//
// The driver configures pin 5 as output and blinks an LED pattern to
// demonstrate active control over a GPIO pin provided by a GKI module.

#ifdef __Fuchsia__

#include <fidl/fuchsia.hardware.gpio/cpp/fidl.h>
#include <lib/driver/component/cpp/driver_base.h>

namespace driverhub {

class GpioLedDriver : public fdf::DriverBase {
 public:
  GpioLedDriver(fdf::DriverStartArgs start_args,
                fdf::UnownedSynchronizedDispatcher driver_dispatcher);
  ~GpioLedDriver() override;

  zx::result<> Start() override;
  void Stop() override;

 private:
  // Connect to the GPIO service and configure the pin as output.
  zx::result<> ConfigureGpio();

  // Blink the LED: toggle between on/off states.
  void BlinkOnce();

  fidl::SyncClient<fuchsia_hardware_gpio::Gpio> gpio_client_;
  bool led_on_ = false;
};

}  // namespace driverhub

#endif  // __Fuchsia__

#endif  // DRIVERHUB_SRC_FUCHSIA_GPIO_LED_DRIVER_H_
