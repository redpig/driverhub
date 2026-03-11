// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Entry point for the DriverHub runner process.
//
// The runner serves three protocols:
// - fuchsia.component.runner.ComponentRunner: launched by the component
//   framework to start .ko module components.
// - fuchsia.driverhub.SymbolRegistry: used by module processes to register
//   and resolve cross-module EXPORT_SYMBOL dependencies.
// - fuchsia.driverhub.DeviceFs: aggregates DevFs/SysFs/ProcFs state for
//   Starnix integration.

#include <fidl/fuchsia.component.runner/cpp/fidl.h>
#include <fidl/fuchsia.driverhub/cpp/fidl.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/component/outgoing/cpp/outgoing_directory.h>

#include <cstdio>

#include "src/runner/device_fs_server.h"
#include "src/runner/ko_runner.h"
#include "src/runner/symbol_registry_server.h"

int main(int argc, char** argv) {
  fprintf(stderr, "driverhub: runner starting\n");

  async::Loop loop(&kAsyncLoopConfigAttachToCurrentThread);
  async_dispatcher_t* dispatcher = loop.dispatcher();

  // Create the outgoing directory to serve our protocols.
  component::OutgoingDirectory outgoing(dispatcher);

  // Create the symbol registry server (shared state for cross-module symbols).
  driverhub::SymbolRegistryServer symbol_registry(dispatcher);

  // Create the KoRunner (ComponentRunner implementation).
  driverhub::KoRunner ko_runner(dispatcher, symbol_registry);

  // Serve fuchsia.component.runner.ComponentRunner.
  auto runner_result = outgoing.AddProtocol<
      fuchsia_component_runner::ComponentRunner>(&ko_runner);
  if (runner_result.is_error()) {
    fprintf(stderr,
            "driverhub: failed to serve ComponentRunner: %s\n",
            runner_result.status_string());
    return 1;
  }

  // Serve fuchsia.driverhub.SymbolRegistry.
  auto registry_result = outgoing.AddProtocol<
      fuchsia_driverhub::SymbolRegistry>(&symbol_registry);
  if (registry_result.is_error()) {
    fprintf(stderr,
            "driverhub: failed to serve SymbolRegistry: %s\n",
            registry_result.status_string());
    return 1;
  }

  // Create the DeviceFs server (aggregates DevFs/SysFs/ProcFs for Starnix).
  driverhub::DeviceFsServer device_fs(dispatcher);

  // Serve fuchsia.driverhub.DeviceFs.
  auto device_fs_result = outgoing.AddProtocol<
      fuchsia_driverhub::DeviceFs>(&device_fs);
  if (device_fs_result.is_error()) {
    fprintf(stderr,
            "driverhub: failed to serve DeviceFs: %s\n",
            device_fs_result.status_string());
    return 1;
  }

  // Serve the outgoing directory.
  auto serve_result = outgoing.ServeFromStartupInfo();
  if (serve_result.is_error()) {
    fprintf(stderr,
            "driverhub: failed to serve outgoing directory: %s\n",
            serve_result.status_string());
    return 1;
  }

  fprintf(stderr, "driverhub: runner ready, serving ComponentRunner, "
          "SymbolRegistry, and DeviceFs\n");

  loop.Run();

  fprintf(stderr, "driverhub: runner shutting down\n");
  return 0;
}
