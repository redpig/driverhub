// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_LOADER_MODULE_LOADER_H_
#define DRIVERHUB_SRC_LOADER_MODULE_LOADER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "driverhub_core.h"

namespace driverhub {

// Metadata extracted from a .ko module's .modinfo section.
struct ModuleInfo {
  std::string name;
  std::string description;
  std::string author;
  std::string license;
  std::string vermagic;
  std::vector<std::string> depends;       // Module dependencies
  std::vector<std::string> aliases;       // Device aliases (modalias)
  std::vector<std::string> compatible;    // OF compatible strings
};

// Represents a loaded .ko module in memory.
// Wraps a Rust DhLoadedModule via FFI.
struct LoadedModule {
  std::string name;
  ModuleInfo info;

  // Entry points resolved from the module's symbol table.
  int (*init_fn)() = nullptr;
  void (*exit_fn)() = nullptr;

  // Opaque Rust handle — freed on destruction.
  DhLoadedModule* rust_handle = nullptr;

  ~LoadedModule();

  // Non-copyable.
  LoadedModule() = default;
  LoadedModule(const LoadedModule&) = delete;
  LoadedModule& operator=(const LoadedModule&) = delete;
  LoadedModule(LoadedModule&& other) noexcept;
  LoadedModule& operator=(LoadedModule&& other) noexcept;
};

class SymbolRegistry;

// Loads Linux GKI .ko modules (ELF ET_REL) via the Rust driverhub-core
// library and resolves their symbols against the KMI shim registry.
class ModuleLoader {
 public:
  // Takes a symbol registry for resolving KMI symbols.
  explicit ModuleLoader(SymbolRegistry& symbols);
  ~ModuleLoader();

  // Load a .ko module from raw ELF bytes. Resolves symbols, applies
  // relocations, and extracts module metadata.
  // Returns nullptr on failure.
  std::unique_ptr<LoadedModule> Load(std::string_view name,
                                     const uint8_t* data, size_t size);

 private:
  DhModuleLoader* rust_loader_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_LOADER_MODULE_LOADER_H_
