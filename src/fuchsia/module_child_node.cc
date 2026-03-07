// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/module_child_node.h"

#include <lib/driver/component/cpp/driver_base.h>
#include <lib/driver/component/cpp/node_add_args.h>
#include <lib/driver/logging/cpp/structured_logger.h>

#include "src/fuchsia/service_bridge.h"

namespace driverhub {

ModuleChildNode::ModuleChildNode(fdf::DriverBase* parent,
                                 std::unique_ptr<LoadedModule> module)
    : parent_(parent), module_(std::move(module)) {}

ModuleChildNode::~ModuleChildNode() {
  if (bound_) {
    Unbind();
  }
}

zx::result<> ModuleChildNode::Bind(
    fidl::SharedClient<fuchsia_driver_framework::Node>& parent_node) {
  if (bound_) {
    return zx::error(ZX_ERR_ALREADY_BOUND);
  }

  FDF_LOGL(INFO, parent_->logger(),
           "Binding module child node: %s", name().c_str());

  // Call the module's init function.
  if (module_->init_fn) {
    FDF_LOGL(INFO, parent_->logger(),
             "Calling init_module for %s", name().c_str());
    int ret = module_->init_fn();
    if (ret != 0) {
      FDF_LOGL(ERROR, parent_->logger(),
               "%s: init_module failed with %d", name().c_str(), ret);
      return zx::error(ZX_ERR_INTERNAL);
    }
    FDF_LOGL(INFO, parent_->logger(),
             "%s: init_module succeeded", name().c_str());
  }

  // Build the child node properties from the module's metadata.
  auto properties = BuildNodeProperties();

  // Build service offers based on the subsystems this module registered with.
  auto offers = BuildServiceOffers();

  // Create the child node args.
  auto args = fuchsia_driver_framework::NodeAddArgs();
  args.name() = name();
  args.properties() = std::move(properties);
  if (!offers.empty()) {
    args.offers() = std::move(offers);
    FDF_LOGL(INFO, parent_->logger(),
             "%s: offering %zu FIDL services for composite binding",
             name().c_str(), args.offers()->size());
  }

  // Add the child node to the DFv2 topology.
  auto endpoints =
      fidl::CreateEndpoints<fuchsia_driver_framework::NodeController>();
  if (endpoints.is_error()) {
    FDF_LOGL(ERROR, parent_->logger(),
             "Failed to create NodeController endpoints: %s",
             endpoints.status_string());
    return endpoints.take_error();
  }

  auto result = parent_node->AddChild(std::move(args),
                                      std::move(endpoints->server),
                                      {});
  if (result.is_error()) {
    FDF_LOGL(ERROR, parent_->logger(),
             "Failed to add child node %s: %s",
             name().c_str(),
             result.error_value().FormatDescription().c_str());
    return zx::error(result.error_value().is_framework_error()
                         ? ZX_ERR_INTERNAL
                         : ZX_ERR_IO);
  }

  controller_.Bind(std::move(endpoints->client),
                   parent_->dispatcher());

  bound_ = true;
  FDF_LOGL(INFO, parent_->logger(),
           "%s: child node created in DFv2 topology", name().c_str());
  return zx::ok();
}

void ModuleChildNode::Unbind() {
  if (!bound_) return;

  FDF_LOGL(INFO, parent_->logger(),
           "Unbinding module child node: %s", name().c_str());

  // Call the module's exit function.
  if (module_->exit_fn) {
    FDF_LOGL(INFO, parent_->logger(),
             "Calling cleanup_module for %s", name().c_str());
    module_->exit_fn();
  }

  // Remove the child node from the DFv2 topology.
  if (controller_) {
    auto result = controller_->Remove();
    if (result.is_error()) {
      FDF_LOGL(WARNING, parent_->logger(),
               "Failed to remove child node %s: %s",
               name().c_str(),
               result.error_value().FormatDescription().c_str());
    }
  }

  bound_ = false;
  FDF_LOGL(INFO, parent_->logger(),
           "%s: unbound", name().c_str());
}

std::vector<fuchsia_driver_framework::NodeProperty>
ModuleChildNode::BuildNodeProperties() const {
  std::vector<fuchsia_driver_framework::NodeProperty> properties;

  // Add the module name as a string property.
  properties.push_back(
      fdf::MakeProperty(/* id */ 0x0001, name()));

  // Add compatible strings from the module's device tree metadata.
  // Each compatible string becomes a bind property that downstream drivers
  // can match against.
  for (const auto& compat : info().compatible) {
    properties.push_back(
        fdf::MakeProperty(/* id */ 0x0002, compat));
  }

  // Add aliases as properties for modalias matching.
  for (const auto& alias : info().aliases) {
    properties.push_back(
        fdf::MakeProperty(/* id */ 0x0003, alias));
  }

  return properties;
}

std::vector<fuchsia_driver_framework::Offer>
ModuleChildNode::BuildServiceOffers() const {
  std::vector<fuchsia_driver_framework::Offer> offers;

  // After module_init() runs, the module may have registered with various
  // subsystems (GPIO, I2C, SPI, USB). The service bridge tracks these
  // registrations. For each subsystem the module registered with, we add
  // a FIDL service offer so downstream drivers can bind to this node for
  // composite device assembly.
  //
  // The service offers enable patterns like:
  //   - A touchscreen driver binding to a GPIO controller's IRQ pin
  //   - A sensor driver binding to an I2C bus provided by a .ko module
  //   - A display driver binding to a SPI device provided by a .ko module

  // Check if any GPIO chips were registered during module_init().
  // If so, offer fuchsia.hardware.gpio.Service.
  if (dh_bridge_find_gpio_chip(0) != nullptr) {
    fuchsia_driver_framework::Offer offer =
        fdf::MakeOffer("fuchsia.hardware.gpio.Service",
                        name());
    offers.push_back(std::move(offer));
  }

  // Check for I2C driver registrations.
  // The module name is used as a heuristic to find matching drivers.
  if (dh_bridge_find_i2c_driver(name().c_str()) != nullptr) {
    fuchsia_driver_framework::Offer offer =
        fdf::MakeOffer("fuchsia.hardware.i2c.Service",
                        name());
    offers.push_back(std::move(offer));
  }

  // Check for SPI driver registrations.
  if (dh_bridge_find_spi_driver(name().c_str()) != nullptr) {
    fuchsia_driver_framework::Offer offer =
        fdf::MakeOffer("fuchsia.hardware.spi.Service",
                        name());
    offers.push_back(std::move(offer));
  }

  // Check for USB driver registrations.
  if (dh_bridge_find_usb_driver(name().c_str()) != nullptr) {
    fuchsia_driver_framework::Offer offer =
        fdf::MakeOffer("fuchsia.hardware.usb.Service",
                        name());
    offers.push_back(std::move(offer));
  }

  return offers;
}

}  // namespace driverhub
