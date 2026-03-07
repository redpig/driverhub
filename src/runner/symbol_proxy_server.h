// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_RUNNER_SYMBOL_PROXY_SERVER_H_
#define DRIVERHUB_SRC_RUNNER_SYMBOL_PROXY_SERVER_H_

#include <fidl/fuchsia.driverhub/cpp/fidl.h>
#include <lib/async/dispatcher.h>
#include <lib/zx/process.h>
#include <lib/zx/vmo.h>

#include <string>

namespace driverhub {

// FIDL server implementing fuchsia.driverhub.SymbolProxy for a single
// exported function symbol.
//
// When a non-colocated module calls an EXPORT_SYMBOL function, the shim
// library in the calling process serializes the arguments and sends them
// over the SymbolProxy channel. This server receives the call, dispatches
// it to the exporting module's process (by creating a thread there that
// invokes the target function), and returns the result.
class SymbolProxyServer
    : public fidl::Server<fuchsia_driverhub::SymbolProxy> {
 public:
  // |target_addr| is the virtual address of the function in the exporting
  // module's process.
  // |target_process| is a borrowed handle to the process (not owned).
  // |target_vmar| is a borrowed handle to the process's root VMAR.
  // |symbol_name| is for logging.
  SymbolProxyServer(uint64_t target_addr,
                    const zx::process& target_process,
                    const zx::vmar& target_vmar,
                    std::string symbol_name);
  ~SymbolProxyServer() override;

  // fuchsia.driverhub.SymbolProxy implementation.
  void Call(CallRequest& request, CallCompleter::Sync& completer) override;

 private:
  uint64_t target_addr_;
  // Borrowed handles — the ManagedProcess owns the lifetime.
  const zx::process& target_process_;
  const zx::vmar& target_vmar_;
  std::string symbol_name_;
};

// Create a SymbolProxy channel pair and bind the server end to the given
// dispatcher. Returns the client end for inclusion in ResolvedSymbol.
fidl::ClientEnd<fuchsia_driverhub::SymbolProxy> CreateSymbolProxy(
    async_dispatcher_t* dispatcher,
    uint64_t target_addr,
    const zx::process& target_process,
    const zx::vmar& target_vmar,
    const std::string& symbol_name);

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_RUNNER_SYMBOL_PROXY_SERVER_H_
