// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_RUNNER_SYMBOL_REGISTRY_SERVER_H_
#define DRIVERHUB_SRC_RUNNER_SYMBOL_REGISTRY_SERVER_H_

#include <fidl/fuchsia.driverhub/cpp/fidl.h>
#include <lib/async/dispatcher.h>
#include <lib/zx/process.h>
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace driverhub {

// Internal representation of a registered symbol.
struct RegisteredSymbol {
  std::string name;
  std::string owning_module;

  // VMO containing the module section where this symbol resides.
  zx::vmo vmo;

  // Byte offset within the VMO.
  uint64_t offset = 0;

  // Virtual address of the symbol in the owning process's address space.
  // Used for function proxy dispatch.
  uint64_t virtual_addr = 0;

  // Function or data.
  fuchsia_driverhub::SymbolKind kind = fuchsia_driverhub::SymbolKind::kData;
};

// Callback to look up a module's process handles for SymbolProxy creation.
// Returns false if the module process is not found.
struct ModuleProcessInfo {
  const zx::process* process = nullptr;
  const zx::vmar* vmar = nullptr;
};
using ModuleProcessLookup =
    std::function<bool(const std::string& module_name, ModuleProcessInfo* out)>;

// FIDL server implementing fuchsia.driverhub.SymbolRegistry.
//
// Runs in the runner process and manages cross-module EXPORT_SYMBOL
// registration and resolution. Thread-safe: multiple module processes
// may call Register/Resolve concurrently.
class SymbolRegistryServer
    : public fidl::Server<fuchsia_driverhub::SymbolRegistry> {
 public:
  explicit SymbolRegistryServer(async_dispatcher_t* dispatcher);
  ~SymbolRegistryServer() override;

  // Set the callback used to look up a module's process handles.
  // Required for creating SymbolProxy channels for function symbols.
  void SetProcessLookup(ModuleProcessLookup lookup);

  // fuchsia.driverhub.SymbolRegistry implementation.
  void Register(RegisterRequest& request,
                RegisterCompleter::Sync& completer) override;
  void Resolve(ResolveRequest& request,
               ResolveCompleter::Sync& completer) override;
  void ResolveBatch(ResolveBatchRequest& request,
                    ResolveBatchCompleter::Sync& completer) override;

  // Remove all symbols registered by a given module.
  // Called when a module process crashes or is stopped.
  void RemoveModule(const std::string& module_name);

  // Direct in-process symbol lookup (no FIDL). Used by LoadModule in the
  // runner process to resolve cross-module symbols during .ko loading.
  // Returns the RegisteredSymbol pointer, or nullptr if not found.
  // The returned pointer is only valid while holding the lock (caller
  // should copy what it needs).
  struct DirectLookupResult {
    bool found = false;
    uint64_t virtual_addr = 0;
    uint64_t offset = 0;
    fuchsia_driverhub::SymbolKind kind = fuchsia_driverhub::SymbolKind::kData;
    // For DATA symbols: VMO handle to map into child. Caller must dup.
    const zx::vmo* vmo = nullptr;
  };
  DirectLookupResult LookupDirect(const std::string& name) const;

  // Returns the number of registered symbols.
  size_t size() const;

 private:
  // Build a ResolvedSymbol FIDL response for a registered symbol.
  fuchsia_driverhub::ResolvedSymbol MakeResolved(
      const RegisteredSymbol& sym) const;

  async_dispatcher_t* dispatcher_;
  ModuleProcessLookup process_lookup_;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, RegisteredSymbol> symbols_
      __attribute__((guarded_by(mutex_)));
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_RUNNER_SYMBOL_REGISTRY_SERVER_H_
