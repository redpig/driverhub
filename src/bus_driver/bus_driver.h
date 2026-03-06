// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_BUS_DRIVER_BUS_DRIVER_H_
#define DRIVERHUB_SRC_BUS_DRIVER_BUS_DRIVER_H_

#include <memory>
#include <string>
#include <vector>

#include "src/loader/module_loader.h"
#include "src/shim/kernel/types.h"
#include "src/symbols/symbol_registry.h"

namespace driverhub {

class ModuleNode;

// Top-level DFv2 bus driver component.
//
// Parses device tree configuration (static or via DT visitor), loads .ko
// modules in dependency order, and creates a DFv2 child node for each module.
class BusDriver {
 public:
  BusDriver();
  ~BusDriver();

  // Initialize the bus driver: set up KMI shims, parse DT, load modules.
  zx_status_t Init();

  // Shut down all modules and release resources.
  void Shutdown();

  // Load a .ko module from raw ELF bytes.
  zx_status_t LoadModule(const std::string& name,
                          const uint8_t* data, size_t size);

  // Load a .ko module from a file path.
  zx_status_t LoadModuleFromFile(const std::string& path);

  size_t module_count() const { return nodes_.size(); }

 private:

  // Create a DFv2 child node for a loaded module.
  zx_status_t CreateModuleNode(std::unique_ptr<LoadedModule> module);

  SymbolRegistry symbols_;
  ModuleLoader loader_;
  std::vector<std::unique_ptr<ModuleNode>> nodes_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_BUS_DRIVER_BUS_DRIVER_H_
