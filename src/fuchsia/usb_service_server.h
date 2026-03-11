// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_USB_SERVICE_SERVER_H_
#define DRIVERHUB_SRC_FUCHSIA_USB_SERVICE_SERVER_H_

// USB FIDL service server.
//
// Bridges fuchsia.hardware.usb/Usb FIDL calls to the usb_driver callbacks
// registered by a loaded .ko module. This enables GKI USB class drivers
// (HID, audio, networking, etc.) to appear as native Fuchsia USB drivers.
//
// The server exposes device descriptor information and allows downstream
// Fuchsia drivers to interact with the USB device through control and
// bulk transfers by delegating to the shim's usb_control_msg /
// usb_bulk_msg functions.

#ifdef __cplusplus
extern "C" {
#endif

struct usb_driver;

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __Fuchsia__

#include <fidl/fuchsia.hardware.usb/cpp/fidl.h>
#include <lib/driver/component/cpp/driver_base.h>

namespace driverhub {

// Serves USB device information backed by a usb_driver.
// Exposes the driver's supported device ID table and name for downstream
// binding and composite assembly.
class UsbServiceServer
    : public fidl::Server<fuchsia_hardware_usb::Usb> {
 public:
  explicit UsbServiceServer(struct usb_driver* drv);
  ~UsbServiceServer() override;

  // fuchsia.hardware.usb/Usb FIDL methods.
  void GetDeviceDescriptor(
      GetDeviceDescriptorCompleter::Sync& completer) override;
  void GetConfigurationDescriptor(
      GetConfigurationDescriptorRequest& request,
      GetConfigurationDescriptorCompleter::Sync& completer) override;
  void GetStringDescriptor(
      GetStringDescriptorRequest& request,
      GetStringDescriptorCompleter::Sync& completer) override;

 private:
  struct usb_driver* drv_;  // Not owned.
};

}  // namespace driverhub

#endif  // __Fuchsia__

#endif  // DRIVERHUB_SRC_FUCHSIA_USB_SERVICE_SERVER_H_
