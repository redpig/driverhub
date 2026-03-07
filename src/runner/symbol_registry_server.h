// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_RUNNER_SYMBOL_REGISTRY_SERVER_H_
#define DRIVERHUB_SRC_RUNNER_SYMBOL_REGISTRY_SERVER_H_

#include <fidl/fuchsia.driverhub/cpp/fidl.h>
#include <lib/async/dispatcher.h>
#include <lib/zx/vmo.h>

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

  // Function or data.
  fuchsia_driverhub::SymbolKind kind = fuchsia_driverhub::SymbolKind::kData;
};

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

  // Returns the number of registered symbols.
  size_t size() const;

 private:
  // Build a ResolvedSymbol FIDL response for a registered symbol.
  fuchsia_driverhub::ResolvedSymbol MakeResolved(
      const RegisteredSymbol& sym) const;

  async_dispatcher_t* dispatcher_;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, RegisteredSymbol> symbols_
      __attribute__((guarded_by(mutex_)));
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_RUNNER_SYMBOL_REGISTRY_SERVER_H_
