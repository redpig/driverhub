// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/module_child_node.h"

#include <lib/driver/component/cpp/driver_base.h>
#include <lib/driver/component/cpp/node_add_args.h>
#include <lib/driver/logging/cpp/structured_logger.h>

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

  // Create the child node args.
  auto args = fuchsia_driver_framework::NodeAddArgs();
  args.name() = name();
  args.properties() = std::move(properties);

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

}  // namespace driverhub
