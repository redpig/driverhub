// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_DRIVERHUB_DRIVER_H_
#define DRIVERHUB_SRC_FUCHSIA_DRIVERHUB_DRIVER_H_

#include <fidl/fuchsia.driver.framework/cpp/fidl.h>
#include <lib/driver/component/cpp/driver_base.h>

#include <memory>
#include <string>
#include <vector>

#include "src/fuchsia/module_child_node.h"
#include "src/loader/module_loader.h"
#include "src/symbols/symbol_registry.h"

namespace driverhub {

// DriverHub DFv2 bus driver.
//
// This is the top-level Fuchsia driver component that:
//   1. Initializes the KMI shim library and symbol registry.
//   2. Reads module configuration (from device tree or component data).
//   3. Loads .ko modules using the ELF loader.
//   4. Creates a DFv2 child node for each loaded module.
//
// Each child node can independently bind to downstream Fuchsia drivers,
// enabling seamless integration of GKI kernel modules into the Fuchsia
// driver topology.
class DriverHubDriver : public fdf::DriverBase {
 public:
  DriverHubDriver(fdf::DriverStartArgs start_args,
                  fdf::UnownedSynchronizedDispatcher driver_dispatcher);
  ~DriverHubDriver() override;

  // fdf::DriverBase interface.
  zx::result<> Start() override;
  void PrepareStop(fdf::PrepareStopCompleter completer) override;
  void Stop() override;

  // Load a .ko module from a VMO and create a child node for it.
  zx::result<> LoadModuleFromVmo(const std::string& name, const zx::vmo& vmo);

  // Load a .ko module from raw bytes and create a child node for it.
  zx::result<> LoadModule(const std::string& name,
                          const uint8_t* data, size_t size);

  size_t module_count() const { return child_nodes_.size(); }

 private:
  // Initialize KMI shim symbols.
  zx::result<> InitShims();

  // Enumerate modules from the component's package data directory.
  zx::result<> EnumerateModules();

  // Create a DFv2 child node for a loaded module.
  zx::result<> CreateChildNode(std::unique_ptr<LoadedModule> module);

  SymbolRegistry symbols_;
  ModuleLoader loader_;
  std::vector<std::unique_ptr<ModuleChildNode>> child_nodes_;

  // FIDL client for the parent node, used to add children.
  fidl::SharedClient<fuchsia_driver_framework::Node> node_client_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_FUCHSIA_DRIVERHUB_DRIVER_H_
