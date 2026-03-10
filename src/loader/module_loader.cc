// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/module_loader.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

#include "src/loader/memory_allocator.h"
#include "src/symbols/symbol_registry.h"

namespace driverhub {

// Bridge between C++ MemoryAllocator and the Rust FFI allocator callbacks.
// The Rust loader calls alloc/dealloc via C function pointers; we route
// those through the thread-local MemoryAllocator instance.
namespace {

thread_local MemoryAllocator* g_allocator = nullptr;
thread_local std::unordered_map<uint8_t*, MemoryAllocation*>* g_alloc_map = nullptr;

uint8_t* AllocatorBridge(size_t size) {
  if (!g_allocator) return nullptr;
  auto* alloc = g_allocator->Allocate(size);
  if (!alloc || !alloc->base) return nullptr;
  if (!g_alloc_map) {
    g_alloc_map = new std::unordered_map<uint8_t*, MemoryAllocation*>();
  }
  (*g_alloc_map)[alloc->base] = alloc;
  return alloc->base;
}

void DeallocatorBridge(uint8_t* ptr, size_t /*size*/) {
  if (!g_alloc_map || !ptr) return;
  auto it = g_alloc_map->find(ptr);
  if (it != g_alloc_map->end()) {
    it->second->Release();
    delete it->second;
    g_alloc_map->erase(it);
  }
}

}  // namespace

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

ModuleLoader::ModuleLoader(SymbolRegistry& symbols, MemoryAllocator& allocator)
    : rust_loader_(dh_module_loader_new_with_allocator(
          symbols.handle(), AllocatorBridge, DeallocatorBridge)) {
  // Store the allocator for the bridge callbacks.
  g_allocator = &allocator;
}

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
