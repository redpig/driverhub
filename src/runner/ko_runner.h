// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_RUNNER_KO_RUNNER_H_
#define DRIVERHUB_SRC_RUNNER_KO_RUNNER_H_

#include <fidl/fuchsia.component.runner/cpp/fidl.h>
#include <lib/async/dispatcher.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "src/runner/process_manager.h"
#include "src/runner/symbol_registry_server.h"

namespace driverhub {

// Metadata extracted from a component's program declaration.
struct ModuleComponentInfo {
  // Path to the .ko binary within the component package.
  std::string ko_binary;

  // Colocation group name, if any. Modules with the same group share a
  // process and resolve EXPORT_SYMBOL directly.
  std::string colocation_group;

  // Ordered list of .ko paths for a colocation group. The group owner
  // declares all members so they can be loaded in dependency order.
  std::vector<std::string> group_modules;
};

// Tracks a running module instance.
struct RunningModule {
  std::string name;
  std::string colocation_group;

  // The ManagedProcess hosting this module. Owned here for isolated modules;
  // for colocated modules, this is nullptr (the process is owned by
  // colocation_groups_).
  std::unique_ptr<ManagedProcess> managed_process;

  // Binding for the ComponentController protocol.
  std::optional<fidl::ServerBinding<fuchsia_component_runner::ComponentController>>
      controller_binding;
};

// DriverHub's component runner for Linux .ko kernel modules.
//
// Implements fuchsia.component.runner.ComponentRunner. For each Start()
// request, the runner:
//   1. Extracts module metadata from ComponentStartInfo.
//   2. Determines the target process (new or existing colocation group).
//   3. Sets up the process with libdriverhub_kmi.so and FIDL channels.
//   4. Loads the .ko via elfldltl and calls module_init().
//   5. Returns a ComponentController for lifecycle management.
class KoRunner : public fidl::Server<fuchsia_component_runner::ComponentRunner> {
 public:
  explicit KoRunner(async_dispatcher_t* dispatcher,
                    SymbolRegistryServer& symbol_registry);
  ~KoRunner() override;

  // fuchsia.component.runner.ComponentRunner implementation.
  void Start(StartRequest& request, StartCompleter::Sync& completer) override;

  // Stop a running module by URL. Called when receiving Stop/Kill via
  // ComponentController.
  void StopModule(const std::string& url, zx_status_t epitaph);

 private:
  // Parse the program dictionary from ComponentStartInfo to extract module
  // metadata (ko_binary, colocation_group, group_modules).
  static zx_status_t ParseProgramInfo(
      const fuchsia_data::Dictionary& program,
      ModuleComponentInfo* out);

  // Find or create a process for the given colocation group.
  // Returns nullptr for isolated (non-colocated) modules.
  ManagedProcess* FindOrCreateGroupProcess(const std::string& group);

  // Start a module in the given process. Loads the .ko, resolves symbols,
  // and calls module_init().
  zx_status_t StartModuleInProcess(
      ManagedProcess& process,
      const ModuleComponentInfo& info,
      const fuchsia_component_runner::ComponentStartInfo& start_info);

  async_dispatcher_t* dispatcher_;
  SymbolRegistryServer& symbol_registry_;
  ProcessManager process_manager_;

  // Colocation groups: group name → shared process.
  std::unordered_map<std::string, std::unique_ptr<ManagedProcess>>
      colocation_groups_;

  // All running module instances, keyed by component URL.
  std::unordered_map<std::string, std::unique_ptr<RunningModule>>
      running_modules_;
};

// Implements fuchsia.component.runner.ComponentController for a single
// running .ko module. Delegates Stop/Kill to KoRunner::StopModule().
class ModuleController
    : public fidl::Server<fuchsia_component_runner::ComponentController> {
 public:
  ModuleController(KoRunner* runner, std::string url);
  ~ModuleController() override;

  void Stop(StopCompleter::Sync& completer) override;
  void Kill(KillCompleter::Sync& completer) override;

 private:
  KoRunner* runner_;
  std::string url_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_RUNNER_KO_RUNNER_H_
