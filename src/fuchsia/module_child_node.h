// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_MODULE_CHILD_NODE_H_
#define DRIVERHUB_SRC_FUCHSIA_MODULE_CHILD_NODE_H_

#include <fidl/fuchsia.driver.framework/cpp/fidl.h>

#include <memory>
#include <string>
#include <vector>

#include "src/loader/module_loader.h"

namespace fdf {
class DriverBase;
}

namespace driverhub {

// A DFv2 child node representing a single loaded GKI .ko module.
//
// Each ModuleChildNode:
//   - Owns the loaded module memory and state.
//   - Exposes DFv2 bind properties derived from the module's compatible strings.
//   - Manages the module lifecycle: module_init() on bind, module_exit() on unbind.
//   - Is independently startable/stoppable without affecting siblings.
//   - Exposes FIDL service offers so downstream drivers can bind to the
//     services provided by the loaded module (e.g., GPIO, I2C, SPI).
//
// This wraps the core ModuleNode logic with DFv2-specific child node management.
class ModuleChildNode {
 public:
  ModuleChildNode(fdf::DriverBase* parent,
                  std::unique_ptr<LoadedModule> module);
  ~ModuleChildNode();

  // Create the DFv2 child node and call module_init().
  zx::result<> Bind(
      fidl::SharedClient<fuchsia_driver_framework::Node>& parent_node);

  // Tear down the child node and call module_exit().
  void Unbind();

  const std::string& name() const { return module_->name; }
  const ModuleInfo& info() const { return module_->info; }
  bool is_bound() const { return bound_; }

 private:
  // Build DFv2 node properties from the module's device tree compatible strings.
  std::vector<fuchsia_driver_framework::NodeProperty> BuildNodeProperties()
      const;

  // Build DFv2 service offers based on subsystems the module registered with.
  std::vector<fuchsia_driver_framework::Offer> BuildServiceOffers() const;

  fdf::DriverBase* parent_;  // Not owned.
  std::unique_ptr<LoadedModule> module_;
  bool bound_ = false;

  // Controller for this child node — used to remove it from the topology.
  fidl::SharedClient<fuchsia_driver_framework::NodeController> controller_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_FUCHSIA_MODULE_CHILD_NODE_H_
