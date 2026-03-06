// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SYMBOLS_SYMBOL_REGISTRY_H_
#define DRIVERHUB_SRC_SYMBOLS_SYMBOL_REGISTRY_H_

#include <string>
#include <string_view>
#include <unordered_map>

namespace driverhub {

// Central registry for symbol resolution across KMI shims and loaded modules.
//
// At startup, all KMI shim symbols are registered. As modules are loaded,
// symbols exported via EXPORT_SYMBOL are added, enabling intermodule
// dependencies to be resolved for subsequently loaded modules.
class SymbolRegistry {
 public:
  SymbolRegistry();
  ~SymbolRegistry();

  // Register a symbol. Overwrites any existing registration for the same name.
  void Register(const std::string& name, void* address);

  // Resolve a symbol by name. Returns nullptr if not found.
  void* Resolve(std::string_view name) const;

  // Returns true if the symbol is registered.
  bool Contains(std::string_view name) const;

  // Number of registered symbols.
  size_t size() const { return symbols_.size(); }

  // Bulk-register all KMI shim symbols (kernel primitives + subsystem APIs).
  void RegisterKmiSymbols();

 private:
  std::unordered_map<std::string, void*> symbols_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_SYMBOLS_SYMBOL_REGISTRY_H_
