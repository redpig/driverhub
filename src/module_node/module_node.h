// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_MODULE_NODE_MODULE_NODE_H_
#define DRIVERHUB_SRC_MODULE_NODE_MODULE_NODE_H_

#include <memory>

#include "src/loader/module_loader.h"
#include "src/shim/kernel/types.h"

namespace driverhub {

// A DFv2 child node representing a single loaded GKI .ko module.
//
// Each ModuleNode owns the loaded module memory and state, exposes bind
// properties derived from device tree compatible strings, and manages the
// module lifecycle (module_init on bind, module_exit on unbind).
//
// Module nodes are independent — a fault in one does not affect siblings.
class ModuleNode {
 public:
  explicit ModuleNode(std::unique_ptr<LoadedModule> module);
  ~ModuleNode();

  // Bind the module: calls module_init() and starts the module.
  zx_status_t Bind();

  // Unbind the module: calls module_exit() and tears down FIDL connections.
  void Unbind();

  const std::string& name() const { return module_->name; }
  const ModuleInfo& info() const { return module_->info; }

 private:
  std::unique_ptr<LoadedModule> module_;
  bool bound_ = false;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_MODULE_NODE_MODULE_NODE_H_
