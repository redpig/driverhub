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

#include "src/loader/memory_allocator.h"

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
struct LoadedModule {
  std::string name;
  ModuleInfo info;

  // Entry points resolved from the module's symbol table.
  int (*init_fn)() = nullptr;
  void (*exit_fn)() = nullptr;

  // Owned memory allocation backing the loaded module sections.
  // Automatically released when the LoadedModule is destroyed.
  std::unique_ptr<MemoryAllocation> allocation;
};

class SymbolRegistry;

// Loads Linux GKI .ko modules (ELF ET_REL) via elfldltl and resolves their
// symbols against the KMI shim library and intermodule symbol registry.
class ModuleLoader {
 public:
  // Takes a symbol registry for resolving KMI symbols and a memory allocator
  // for section memory. The allocator determines the backend: mmap on host,
  // VMO on Fuchsia.
  ModuleLoader(SymbolRegistry& symbols, MemoryAllocator& allocator);
  ~ModuleLoader();

  // Load a .ko module from raw ELF bytes. Resolves symbols, applies
  // relocations, and extracts module metadata.
  // Returns nullptr on failure.
  std::unique_ptr<LoadedModule> Load(std::string_view name,
                                     const uint8_t* data, size_t size);

 private:
  SymbolRegistry& symbols_;
  MemoryAllocator& allocator_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_LOADER_MODULE_LOADER_H_
