// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/driverhub_driver.h"

#include <lib/driver/component/cpp/driver_export.h>
#include <lib/driver/logging/cpp/structured_logger.h>

#include <unordered_map>

#include "src/dt/dt_provider.h"
#include "src/fuchsia/module_child_node.h"
#include "src/fuchsia/module_enumerator.h"
#include "src/fuchsia/resource_provider.h"
#include "src/fuchsia/service_bridge.h"
#include "src/loader/dependency_sort.h"
#include "src/loader/modinfo_parser.h"
#include "src/module_node/module_node.h"

namespace driverhub {

// Forward declaration — drains EXPORT_SYMBOL calls from module.cc.
std::unordered_map<std::string, void*> DrainPendingExports();

// Service bridge callback — creates a DFv2 child node for a subsystem
// service (GPIO pin, I2C bus, SPI device, etc.) that a .ko module
// registered during module_init().
//
// This is the glue between Linux subsystem registration functions
// (gpiochip_add_data, i2c_add_driver, etc.) and the Fuchsia DFv2
// topology. Each service child node can be bound by downstream drivers
// for composite device assembly.
static int BridgeAddChild(dh_bridge_ctx_t ctx, int service_type,
                           const char* name, int instance_id,
                           void* provider_ptr) {
  (void)ctx;
  (void)service_type;
  (void)instance_id;
  (void)provider_ptr;

  // In the bus driver model, per-service child nodes are created as
  // additional children of the bus driver's parent node. The service
  // bridge logs the registration; the actual DFv2 child node and FIDL
  // service binding happens when the module's ModuleChildNode is
  // created (see ModuleChildNode::BuildServiceOffers).
  //
  // In the runner model (Phase 2+), the KoRunner creates dedicated
  // component instances with proper capability routing.
  fprintf(stderr,
          "driverhub: bridge: service child '%s' registered "
          "(type=%d)\n",
          name, service_type);
  return 0;
}

static void BridgeRemoveChild(dh_bridge_ctx_t ctx, int service_type,
                                const char* name, void* provider_ptr) {
  (void)ctx;
  (void)service_type;
  (void)provider_ptr;
  fprintf(stderr,
          "driverhub: bridge: service child '%s' removed (type=%d)\n",
          name, service_type);
}

DriverHubDriver::DriverHubDriver(
    fdf::DriverStartArgs start_args,
    fdf::UnownedSynchronizedDispatcher driver_dispatcher)
    : fdf::DriverBase("driverhub", std::move(start_args),
                      std::move(driver_dispatcher)),
      loader_(symbols_) {}

DriverHubDriver::~DriverHubDriver() = default;

zx::result<> DriverHubDriver::Start() {
  FDF_LOG(INFO, "DriverHub bus driver starting");

  // Acquire Fuchsia kernel resources for hardware access (I/O ports, MMIO, VMEX).
  // Failures are non-fatal — features requiring unavailable resources simply won't work.
  dh_resources_init();

  // Initialize the KMI shim symbol registry.
  auto status = InitShims();
  if (status.is_error()) {
    FDF_LOG(ERROR, "Failed to initialize KMI shims: %s",
            status.status_string());
    return status;
  }

  FDF_LOG(INFO, "DriverHub: %zu KMI symbols registered", symbols_.size());

  // Initialize the service bridge. This connects subsystem registrations
  // (gpiochip_add_data, i2c_add_driver, etc.) to DFv2 child node creation.
  dh_bridge_init(this, BridgeAddChild, BridgeRemoveChild);

  // Bind to the parent node so we can add children.
  node_client_.Bind(std::move(node()), dispatcher());

  // Enumerate and load modules from the package data directory.
  status = EnumerateModules();
  if (status.is_error()) {
    FDF_LOG(WARNING, "Module enumeration returned: %s",
            status.status_string());
    // Non-fatal — the driver can still function, modules may be loaded
    // dynamically later.
  }

  FDF_LOG(INFO, "DriverHub: %zu module(s) loaded", child_nodes_.size());
  return zx::ok();
}

void DriverHubDriver::PrepareStop(fdf::PrepareStopCompleter completer) {
  FDF_LOG(INFO, "DriverHub preparing to stop (%zu modules)",
          child_nodes_.size());

  // Unbind all child nodes in reverse order.
  for (auto it = child_nodes_.rbegin(); it != child_nodes_.rend(); ++it) {
    (*it)->Unbind();
  }
  child_nodes_.clear();

  // Tear down the service bridge.
  dh_bridge_teardown();

  completer(zx::ok());
}

void DriverHubDriver::Stop() {
  FDF_LOG(INFO, "DriverHub stopped");
}

zx::result<> DriverHubDriver::InitShims() {
  symbols_.RegisterKmiSymbols();
  return zx::ok();
}

zx::result<> DriverHubDriver::EnumerateModules() {
  FDF_LOG(INFO, "Module enumeration: scanning package data");

  // Step 1: Read device tree configuration to determine which modules to load
  // and their resource assignments. Try the DT visitor service first, then
  // fall back to a JSON manifest or static .dtb blob.
  VisitorDtProvider dt_provider;
  auto dt_entries = dt_provider.GetModuleEntries();
  FDF_LOG(INFO, "DT configuration: %zu module entries", dt_entries.size());

  // Step 2: Enumerate .ko files from the package data directory.
  auto modules = ModuleEnumerator::EnumerateFromDirectory(
      "/pkg/data/modules");

  if (modules.empty()) {
    FDF_LOG(WARNING, "No .ko modules found in /pkg/data/modules/");
    return zx::ok();
  }

  FDF_LOG(INFO, "Found %zu .ko module(s) in package data", modules.size());

  // Step 3: Parse modinfo from each module to extract dependency information,
  // then topologically sort by dependencies so that depended-upon modules are
  // loaded first.
  std::vector<std::pair<std::string, ModuleInfo>> module_infos;
  for (const auto& entry : modules) {
    ModuleInfo info;
    ExtractModuleInfo(entry.data.data(), entry.data.size(), &info);
    module_infos.emplace_back(entry.name, std::move(info));
  }

  std::vector<size_t> load_order;
  if (!TopologicalSortModules(module_infos, &load_order)) {
    FDF_LOG(WARNING, "Dependency cycle detected; loading in discovery order");
    load_order.resize(modules.size());
    for (size_t i = 0; i < modules.size(); i++) {
      load_order[i] = i;
    }
  }

  // Step 4: Load each module in dependency order and create child nodes.
  size_t loaded = 0;
  for (size_t idx : load_order) {
    const auto& entry = modules[idx];
    FDF_LOG(INFO, "Loading module [%zu/%zu]: %s (%zu bytes)",
            loaded + 1, modules.size(), entry.name.c_str(),
            entry.data.size());

    auto status = LoadModule(entry.name, entry.data.data(), entry.data.size());
    if (status.is_error()) {
      FDF_LOG(ERROR, "Failed to load module %s: %s",
              entry.name.c_str(), status.status_string());
      // Continue loading remaining modules — one failure shouldn't block others.
      continue;
    }
    loaded++;
  }

  FDF_LOG(INFO, "Module enumeration complete: %zu/%zu loaded successfully",
          loaded, modules.size());
  return zx::ok();
}

zx::result<> DriverHubDriver::LoadModuleFromVmo(
    const std::string& name, const zx::vmo& vmo) {
  // Get VMO size.
  uint64_t vmo_size;
  zx_status_t status = vmo.get_size(&vmo_size);
  if (status != ZX_OK) {
    FDF_LOG(ERROR, "Failed to get VMO size for %s: %s",
            name.c_str(), zx_status_get_string(status));
    return zx::error(status);
  }

  // Read VMO contents into a buffer.
  std::vector<uint8_t> data(vmo_size);
  status = vmo.read(data.data(), 0, vmo_size);
  if (status != ZX_OK) {
    FDF_LOG(ERROR, "Failed to read VMO for %s: %s",
            name.c_str(), zx_status_get_string(status));
    return zx::error(status);
  }

  return LoadModule(name, data.data(), data.size());
}

zx::result<> DriverHubDriver::LoadModule(
    const std::string& name, const uint8_t* data, size_t size) {
  FDF_LOG(INFO, "Loading module %s (%zu bytes)", name.c_str(), size);

  auto module = loader_.Load(name, data, size);
  if (!module) {
    FDF_LOG(ERROR, "Failed to load module %s", name.c_str());
    return zx::error(ZX_ERR_IO);
  }

  return CreateChildNode(std::move(module));
}

zx::result<> DriverHubDriver::CreateChildNode(
    std::unique_ptr<LoadedModule> module) {
  auto child = std::make_unique<ModuleChildNode>(this, std::move(module));

  auto status = child->Bind(node_client_);
  if (status.is_error()) {
    FDF_LOG(ERROR, "Failed to bind child node %s: %s",
            child->name().c_str(), status.status_string());
    return status;
  }

  // Drain any symbols the module exported during init.
  auto exports = DrainPendingExports();
  for (auto& [sym_name, addr] : exports) {
    symbols_.Register(sym_name, addr);
    FDF_LOG(DEBUG, "Registered intermodule symbol: %s", sym_name.c_str());
  }

  child_nodes_.push_back(std::move(child));
  return zx::ok();
}

}  // namespace driverhub

FUCHSIA_DRIVER_EXPORT(driverhub::DriverHubDriver);
