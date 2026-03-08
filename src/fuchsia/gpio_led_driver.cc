// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/gpio_led_driver.h"

#ifdef __Fuchsia__

#include <lib/driver/component/cpp/driver_export.h>
#include <lib/driver/logging/cpp/structured_logger.h>

namespace driverhub {

GpioLedDriver::GpioLedDriver(
    fdf::DriverStartArgs start_args,
    fdf::UnownedSynchronizedDispatcher driver_dispatcher)
    : fdf::DriverBase("gpio-led", std::move(start_args),
                      std::move(driver_dispatcher)) {}

GpioLedDriver::~GpioLedDriver() = default;

zx::result<> GpioLedDriver::Start() {
  FDF_LOG(INFO, "GPIO LED driver starting — native DFv2 composite consumer");

  auto result = ConfigureGpio();
  if (result.is_error()) {
    FDF_LOG(ERROR, "Failed to configure GPIO: %s", result.status_string());
    return result;
  }

  // Blink the LED a few times to demonstrate active control.
  for (int i = 0; i < 3; i++) {
    BlinkOnce();
  }

  FDF_LOG(INFO, "GPIO LED driver started — pin configured as output, LED on");
  return zx::ok();
}

void GpioLedDriver::Stop() {
  // Turn off the LED on shutdown.
  if (gpio_client_.is_valid()) {
    auto result = gpio_client_->Write(0);
    if (result.is_ok()) {
      FDF_LOG(INFO, "GPIO LED driver: LED off (shutdown)");
    }
  }
  FDF_LOG(INFO, "GPIO LED driver stopped");
}

zx::result<> GpioLedDriver::ConfigureGpio() {
  // Connect to the fuchsia.hardware.gpio/Gpio service provided by the
  // gpio-5 child node of the DriverHub bus driver. This service is backed
  // by the GpioServiceServer which routes FIDL calls to the gpio_chip
  // callbacks of the .ko GPIO controller module.
  auto client_end = incoming()->Connect<fuchsia_hardware_gpio::Service::Device>("default");
  if (client_end.is_error()) {
    FDF_LOG(ERROR, "Failed to connect to GPIO service: %s",
            client_end.status_string());
    return client_end.take_error();
  }

  gpio_client_ = fidl::SyncClient(std::move(*client_end));

  // Configure as output with initial value 0 (LED off).
  auto config_result = gpio_client_->ConfigOut(0);
  if (config_result.is_error()) {
    FDF_LOG(ERROR, "Failed to configure GPIO as output: %s",
            config_result.error_value().FormatDescription().c_str());
    return zx::error(ZX_ERR_IO);
  }

  FDF_LOG(INFO, "GPIO LED: pin configured as output (initial=off)");
  return zx::ok();
}

void GpioLedDriver::BlinkOnce() {
  if (!gpio_client_.is_valid()) return;

  // Toggle LED state.
  led_on_ = !led_on_;
  auto result = gpio_client_->Write(led_on_ ? 1 : 0);
  if (result.is_ok()) {
    FDF_LOG(INFO, "GPIO LED: %s", led_on_ ? "ON" : "OFF");
  }

  // Read back to verify.
  auto read_result = gpio_client_->Read();
  if (read_result.is_ok()) {
    FDF_LOG(INFO, "GPIO LED: readback = %u", read_result->value());
  }
}

}  // namespace driverhub

FUCHSIA_DRIVER_EXPORT(driverhub::GpioLedDriver);

#endif  // __Fuchsia__
