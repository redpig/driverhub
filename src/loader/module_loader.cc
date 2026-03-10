// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/module_loader.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "src/symbols/symbol_registry.h"

namespace driverhub {

// LoadedModule destructor — frees the underlying Rust handle.
LoadedModule::~LoadedModule() {
  if (rust_handle) {
    dh_loaded_module_free(rust_handle);
  }
}

LoadedModule::LoadedModule(LoadedModule&& other) noexcept
    : name(std::move(other.name)),
      info(std::move(other.info)),
      init_fn(other.init_fn),
      exit_fn(other.exit_fn),
      rust_handle(other.rust_handle) {
  other.init_fn = nullptr;
  other.exit_fn = nullptr;
  other.rust_handle = nullptr;
}

LoadedModule& LoadedModule::operator=(LoadedModule&& other) noexcept {
  if (this != &other) {
    if (rust_handle) {
      dh_loaded_module_free(rust_handle);
    }
    name = std::move(other.name);
    info = std::move(other.info);
    init_fn = other.init_fn;
    exit_fn = other.exit_fn;
    rust_handle = other.rust_handle;
    other.init_fn = nullptr;
    other.exit_fn = nullptr;
    other.rust_handle = nullptr;
  }
  return *this;
}

ModuleLoader::ModuleLoader(SymbolRegistry& symbols)
    : rust_loader_(dh_module_loader_new(symbols.handle())) {}

ModuleLoader::~ModuleLoader() {
  if (rust_loader_) {
    dh_module_loader_free(rust_loader_);
  }
}

std::unique_ptr<LoadedModule> ModuleLoader::Load(std::string_view name,
                                                  const uint8_t* data,
                                                  size_t size) {
  std::string name_str(name);
  DhLoadedModule* rust_mod = dh_module_loader_load(
      rust_loader_, name_str.c_str(), data, size);
  if (!rust_mod) {
    return nullptr;
  }

  auto module = std::make_unique<LoadedModule>();
  module->rust_handle = rust_mod;
  module->name = name_str;

  // Extract module info from the Rust side.
  DhModuleInfo* rust_info = dh_loaded_module_info(rust_mod);
  if (rust_info) {
    if (rust_info->name) module->info.name = rust_info->name;
    if (rust_info->description) module->info.description = rust_info->description;
    if (rust_info->license) module->info.license = rust_info->license;
    if (rust_info->vermagic) module->info.vermagic = rust_info->vermagic;
    for (size_t i = 0; i < rust_info->depends_count; i++) {
      if (rust_info->depends[i]) {
        module->info.depends.push_back(rust_info->depends[i]);
      }
    }
    for (size_t i = 0; i < rust_info->aliases_count; i++) {
      if (rust_info->aliases[i]) {
        module->info.aliases.push_back(rust_info->aliases[i]);
      }
    }
    dh_module_info_free(rust_info);
  }
  if (module->info.name.empty()) {
    module->info.name = name_str;
  }

  // Extract entry points.
  module->init_fn = reinterpret_cast<int(*)()>(
      reinterpret_cast<void*>(dh_loaded_module_init_fn(rust_mod)));
  module->exit_fn = reinterpret_cast<void(*)()>(
      reinterpret_cast<void*>(dh_loaded_module_exit_fn(rust_mod)));

  return module;
}

}  // namespace driverhub
