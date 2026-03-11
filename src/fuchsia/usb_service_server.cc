// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/usb_service_server.h"

#include <cstdio>

#include "src/shim/subsystem/usb.h"

#ifdef __Fuchsia__

namespace driverhub {

UsbServiceServer::UsbServiceServer(struct usb_driver* drv) : drv_(drv) {}

UsbServiceServer::~UsbServiceServer() = default;

void UsbServiceServer::GetDeviceDescriptor(
    GetDeviceDescriptorCompleter::Sync& completer) {
  if (!drv_ || !drv_->id_table) {
    completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }

  // Build a device descriptor from the driver's ID table.
  // The first entry in the ID table provides the primary device identity.
  const auto& id = drv_->id_table[0];

  fuchsia_hardware_usb::DeviceDescriptor desc;
  desc.id_vendor() = id.idVendor;
  desc.id_product() = id.idProduct;
  desc.b_device_class() = id.bDeviceClass;
  desc.b_device_sub_class() = id.bDeviceSubClass;
  desc.b_device_protocol() = id.bDeviceProtocol;

  completer.Reply(zx::ok(std::move(desc)));
}

void UsbServiceServer::GetConfigurationDescriptor(
    GetConfigurationDescriptorRequest& request,
    GetConfigurationDescriptorCompleter::Sync& completer) {
  // Configuration descriptors require actual USB device communication.
  // The shim layer doesn't have a connected USB device yet at registration
  // time — this is populated when a physical device is matched.
  completer.Reply(zx::error(ZX_ERR_NOT_SUPPORTED));
}

void UsbServiceServer::GetStringDescriptor(
    GetStringDescriptorRequest& request,
    GetStringDescriptorCompleter::Sync& completer) {
  // Return the driver name for index 0 (product string).
  if (request.index() == 0 && drv_ && drv_->name) {
    completer.Reply(zx::ok(std::string(drv_->name)));
    return;
  }
  completer.Reply(zx::error(ZX_ERR_NOT_FOUND));
}

}  // namespace driverhub

#endif  // __Fuchsia__
