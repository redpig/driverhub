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

#include "src/fuchsia/module_enumerator.h"
#include "src/loader/dependency_sort.h"
#include "src/loader/memory_allocator.h"
#include "src/loader/modinfo_parser.h"
#include "src/module_node/module_node.h"

// Platform-specific allocator selection.
#if defined(__Fuchsia__)
#include "src/fuchsia/vmo_module_loader.h"
#else
#include "src/loader/mmap_allocator.h"
#endif

namespace driverhub {

// Forward declaration — drains EXPORT_SYMBOL calls from module.cc.
std::unordered_map<std::string, void*> DrainPendingExports();

namespace {

std::unique_ptr<MemoryAllocator> CreatePlatformAllocator() {
#if defined(__Fuchsia__)
  return std::make_unique<VmoAllocator>();
#else
  return std::make_unique<MmapAllocator>();
#endif
}

}  // namespace

BusDriver::BusDriver()
    : allocator_(CreatePlatformAllocator()),
      loader_(symbols_, *allocator_) {}

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

zx_status_t BusDriver::LoadModulesFromDirectory(const std::string& dir_path) {
  // Step 1: Enumerate all .ko files in the directory.
  auto entries = ModuleEnumerator::EnumerateFromDirectory(dir_path);
  if (entries.empty()) {
    fprintf(stderr, "driverhub: no modules found in %s\n", dir_path.c_str());
    return -1;
  }
  fprintf(stderr, "driverhub: enumerated %zu modules from %s\n",
          entries.size(), dir_path.c_str());

  // Step 2: Extract modinfo from each module to build dependency graph.
  // Uses lightweight ELF parser — no allocation, relocation, or execution.
  std::vector<std::pair<std::string, ModuleInfo>> module_infos;
  for (const auto& entry : entries) {
    ModuleInfo info;
    info.name = entry.name;
    ExtractModuleInfo(entry.data.data(), entry.data.size(), &info);
    module_infos.push_back({entry.name, info});
  }

  // Step 3: Topologically sort by dependencies.
  std::vector<size_t> load_order;
  if (!TopologicalSortModules(module_infos, &load_order)) {
    fprintf(stderr, "driverhub: dependency cycle detected, aborting\n");
    return -1;
  }

  fprintf(stderr, "driverhub: load order determined for %zu modules\n",
          load_order.size());

  // Step 4: Load modules in dependency order.
  int failures = 0;
  for (size_t idx : load_order) {
    const auto& entry = entries[idx];
    zx_status_t status = LoadModule(entry.name, entry.data.data(),
                                    entry.data.size());
    if (status != 0) {
      fprintf(stderr, "driverhub: failed to load %s (continuing)\n",
              entry.name.c_str());
      failures++;
    }
  }

  fprintf(stderr, "driverhub: loaded %zu/%zu modules (%d failures)\n",
          load_order.size() - failures, load_order.size(), failures);
  return failures > 0 ? -1 : 0;
}

zx_status_t BusDriver::LoadModulesFromFiles(
    const std::vector<std::string>& paths) {
  // Step 1: Read all module files and extract modinfo.
  struct ModuleFile {
    std::string name;
    std::vector<uint8_t> data;
  };
  std::vector<ModuleFile> files;
  std::vector<std::pair<std::string, ModuleInfo>> module_infos;

  for (const auto& path : paths) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      fprintf(stderr, "driverhub: cannot open %s\n", path.c_str());
      continue;
    }
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    // Derive module name from filename.
    std::string name = path;
    auto slash = name.rfind('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() > 3 && name.substr(name.size() - 3) == ".ko")
      name = name.substr(0, name.size() - 3);

    ModuleInfo info;
    info.name = name;
    ExtractModuleInfo(data.data(), data.size(), &info);
    module_infos.push_back({name, info});
    files.push_back({name, std::move(data)});
  }

  // Step 2: Topologically sort by dependencies.
  std::vector<size_t> load_order;
  if (!TopologicalSortModules(module_infos, &load_order)) {
    fprintf(stderr, "driverhub: dependency cycle detected, loading in "
                    "original order\n");
    load_order.clear();
    for (size_t i = 0; i < files.size(); i++) load_order.push_back(i);
  }

  fprintf(stderr, "driverhub: load order determined for %zu modules\n",
          load_order.size());

  // Step 3: Load modules in dependency order.
  int failures = 0;
  for (size_t idx : load_order) {
    const auto& mf = files[idx];
    fprintf(stderr, "\n--- Loading %s ---\n", mf.name.c_str());
    zx_status_t status = LoadModule(mf.name, mf.data.data(), mf.data.size());
    if (status != 0) {
      fprintf(stderr, "driverhub: failed to load %s (continuing)\n",
              mf.name.c_str());
      failures++;
    }
  }

  fprintf(stderr, "driverhub: loaded %zu/%zu modules (%d failures)\n",
          load_order.size() - failures, load_order.size(), failures);
  return failures > 0 ? -1 : 0;
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
