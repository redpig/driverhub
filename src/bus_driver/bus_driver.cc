// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bus_driver/bus_driver.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/module_node/module_node.h"

namespace driverhub {

// Forward declaration — drains EXPORT_SYMBOL calls from module.cc.
std::unordered_map<std::string, void*> DrainPendingExports();

BusDriver::BusDriver() : loader_(symbols_) {}

BusDriver::~BusDriver() {
  Shutdown();
}

zx_status_t BusDriver::Init() {
  fprintf(stderr, "driverhub: bus driver initializing\n");

  // Register all KMI shim symbols so modules can resolve against them.
  symbols_.RegisterKmiSymbols();

  fprintf(stderr, "driverhub: bus driver ready (%zu KMI symbols registered)\n",
          symbols_.size());
  return 0;  // ZX_OK
}

void BusDriver::Shutdown() {
  fprintf(stderr, "driverhub: bus driver shutting down (%zu modules)\n",
          nodes_.size());

  // Unbind in reverse order to respect dependency ordering.
  for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
    (*it)->Unbind();
  }
  nodes_.clear();

  fprintf(stderr, "driverhub: bus driver shutdown complete\n");
}

zx_status_t BusDriver::LoadModule(const std::string& name,
                                   const uint8_t* data, size_t size) {
  fprintf(stderr, "driverhub: loading module %s (%zu bytes)\n",
          name.c_str(), size);

  auto module = loader_.Load(name, data, size);
  if (!module) {
    fprintf(stderr, "driverhub: failed to load %s\n", name.c_str());
    return -1;  // ZX_ERR_IO
  }

  return CreateModuleNode(std::move(module));
}

zx_status_t BusDriver::LoadModuleFromFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    fprintf(stderr, "driverhub: cannot open %s\n", path.c_str());
    return -1;
  }

  size_t size = file.tellg();
  file.seekg(0);

  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char*>(data.data()), size);

  // Derive module name from filename.
  std::string name = path;
  auto slash = name.rfind('/');
  if (slash != std::string::npos) {
    name = name.substr(slash + 1);
  }
  // Strip .ko extension.
  if (name.size() > 3 && name.substr(name.size() - 3) == ".ko") {
    name = name.substr(0, name.size() - 3);
  }

  return LoadModule(name, data.data(), data.size());
}

zx_status_t BusDriver::CreateModuleNode(
    std::unique_ptr<LoadedModule> module) {
  auto node = std::make_unique<ModuleNode>(std::move(module));

  zx_status_t status = node->Bind();
  if (status != 0) {
    fprintf(stderr, "driverhub: failed to bind %s\n", node->name().c_str());
    return status;
  }

  // Drain any symbols the module exported during init.
  auto exports = DrainPendingExports();
  for (auto& [sym_name, addr] : exports) {
    symbols_.Register(sym_name, addr);
    fprintf(stderr, "driverhub: registered intermodule symbol %s\n",
            sym_name.c_str());
  }

  nodes_.push_back(std::move(node));
  return 0;  // ZX_OK
}

}  // namespace driverhub
