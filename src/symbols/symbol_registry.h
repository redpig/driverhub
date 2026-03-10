// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SYMBOLS_SYMBOL_REGISTRY_H_
#define DRIVERHUB_SRC_SYMBOLS_SYMBOL_REGISTRY_H_

#include <string>
#include <string_view>

#include "driverhub_core.h"

namespace driverhub {

// Central registry for symbol resolution across KMI shims and loaded modules.
//
// Thin C++ wrapper around the Rust DhSymbolRegistry via FFI.
class SymbolRegistry {
 public:
  SymbolRegistry();
  ~SymbolRegistry();

  // Non-copyable, non-movable (owns a Rust heap allocation).
  SymbolRegistry(const SymbolRegistry&) = delete;
  SymbolRegistry& operator=(const SymbolRegistry&) = delete;

  // Register a symbol. Overwrites any existing registration for the same name.
  void Register(const std::string& name, void* address);

  // Resolve a symbol by name. Returns nullptr if not found.
  void* Resolve(std::string_view name) const;

  // Returns true if the symbol is registered.
  bool Contains(std::string_view name) const;

  // Number of registered symbols.
  size_t size() const;

  // Bulk-register all KMI shim symbols (kernel primitives + subsystem APIs).
  void RegisterKmiSymbols();

  // Access the underlying Rust handle (for passing to Rust FFI functions).
  DhSymbolRegistry* handle() { return handle_; }
  const DhSymbolRegistry* handle() const { return handle_; }

 private:
  DhSymbolRegistry* handle_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_SYMBOLS_SYMBOL_REGISTRY_H_
