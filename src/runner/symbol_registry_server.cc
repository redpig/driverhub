// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runner/symbol_registry_server.h"

#include <zircon/status.h>

#include <cstdio>

namespace driverhub {

SymbolRegistryServer::SymbolRegistryServer(async_dispatcher_t* dispatcher)
    : dispatcher_(dispatcher) {}

SymbolRegistryServer::~SymbolRegistryServer() = default;

void SymbolRegistryServer::Register(RegisterRequest& request,
                                     RegisterCompleter::Sync& completer) {
  const auto& module_name = request.module_name();
  const auto& symbols = request.symbols();

  std::lock_guard lock(mutex_);

  size_t registered = 0;
  for (const auto& sym : symbols) {
    if (!sym.name().has_value()) {
      continue;
    }

    RegisteredSymbol entry;
    entry.name = sym.name().value();
    entry.owning_module = std::string(module_name);

    if (sym.vmo().has_value()) {
      // Duplicate the VMO handle so we own a copy.
      zx::vmo dup;
      sym.vmo().value().duplicate(ZX_RIGHT_SAME_RIGHTS, &dup);
      entry.vmo = std::move(dup);
    }

    if (sym.offset().has_value()) {
      entry.offset = sym.offset().value();
    }

    if (sym.kind().has_value()) {
      entry.kind = sym.kind().value();
    }

    fprintf(stderr, "driverhub: symbol_registry: registered %s from %s "
            "(kind=%s, offset=0x%lx)\n",
            entry.name.c_str(), module_name.data(),
            entry.kind == fuchsia_driverhub::SymbolKind::kFunction
                ? "function" : "data",
            entry.offset);

    symbols_[entry.name] = std::move(entry);
    registered++;
  }

  fprintf(stderr, "driverhub: symbol_registry: registered %zu symbols "
          "from %s (total: %zu)\n",
          registered, std::string(module_name).c_str(), symbols_.size());

  completer.Reply(fit::ok());
}

void SymbolRegistryServer::Resolve(ResolveRequest& request,
                                    ResolveCompleter::Sync& completer) {
  const auto& name = request.name();

  std::lock_guard lock(mutex_);

  auto it = symbols_.find(std::string(name));
  if (it == symbols_.end()) {
    completer.Reply(fit::error(ZX_ERR_NOT_FOUND));
    return;
  }

  completer.Reply(fit::ok(MakeResolved(it->second)));
}

void SymbolRegistryServer::ResolveBatch(ResolveBatchRequest& request,
                                         ResolveBatchCompleter::Sync& completer) {
  const auto& names = request.names();

  std::lock_guard lock(mutex_);

  std::vector<fuchsia_driverhub::ResolvedSymbol> results;
  results.reserve(names.size());

  for (const auto& name : names) {
    auto it = symbols_.find(std::string(name));
    if (it != symbols_.end()) {
      results.push_back(MakeResolved(it->second));
    }
  }

  completer.Reply(fit::ok(std::move(results)));
}

void SymbolRegistryServer::RemoveModule(const std::string& module_name) {
  std::lock_guard lock(mutex_);

  size_t removed = 0;
  for (auto it = symbols_.begin(); it != symbols_.end();) {
    if (it->second.owning_module == module_name) {
      fprintf(stderr, "driverhub: symbol_registry: removing %s (module %s)\n",
              it->first.c_str(), module_name.c_str());
      it = symbols_.erase(it);
      removed++;
    } else {
      ++it;
    }
  }

  fprintf(stderr, "driverhub: symbol_registry: removed %zu symbols "
          "from %s (remaining: %zu)\n",
          removed, module_name.c_str(), symbols_.size());
}

size_t SymbolRegistryServer::size() const {
  std::lock_guard lock(mutex_);
  return symbols_.size();
}

fuchsia_driverhub::ResolvedSymbol SymbolRegistryServer::MakeResolved(
    const RegisteredSymbol& sym) const {
  fuchsia_driverhub::ResolvedSymbol resolved;
  resolved.name() = sym.name;
  resolved.kind() = sym.kind;
  resolved.offset() = sym.offset;

  if (sym.vmo.is_valid()) {
    zx::vmo dup;
    sym.vmo.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup);
    resolved.vmo() = std::move(dup);
  }

  // TODO(driverhub): For FUNCTION symbols, create a SymbolProxy channel
  // pair and return the client end. The server end is dispatched to the
  // exporting module's process.

  return resolved;
}

}  // namespace driverhub
