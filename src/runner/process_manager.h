// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_RUNNER_PROCESS_MANAGER_H_
#define DRIVERHUB_SRC_RUNNER_PROCESS_MANAGER_H_

#include <fidl/fuchsia.component.runner/cpp/fidl.h>
#include <lib/async/dispatcher.h>
#include <lib/zx/process.h>
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace driverhub {

class SymbolRegistryServer;

// Represents a process managed by the runner. Each process hosts one or more
// .ko modules (multiple if they share a colocation group).
class ManagedProcess {
 public:
  ManagedProcess(std::string name, zx::process process, zx::vmar root_vmar);
  ~ManagedProcess();

  ManagedProcess(const ManagedProcess&) = delete;
  ManagedProcess& operator=(const ManagedProcess&) = delete;

  const std::string& name() const { return name_; }
  zx::process& process_handle() { return process_; }

  // Inject libdriverhub_kmi.so into the process's address space.
  // Maps the shared library VMO with appropriate permissions.
  zx_status_t InjectShimLibrary();

  // Read the .ko binary from the component's namespace.
  zx_status_t ReadModuleBinary(
      const fuchsia_component_runner::ComponentStartInfo& start_info,
      const std::string& ko_path,
      std::vector<uint8_t>* out);

  // Load a .ko module into this process's VMAR. Parses ELF, allocates
  // sections, resolves symbols against the shim library and the registry,
  // and applies relocations.
  zx_status_t LoadModule(const uint8_t* data, size_t size,
                         SymbolRegistryServer& registry);

  // Call the loaded module's init_module() entry point.
  zx_status_t CallModuleInit();

  // Call the loaded module's cleanup_module() entry point.
  zx_status_t CallModuleExit();

  // Install an async wait for ZX_PROCESS_TERMINATED. Invokes `callback`
  // when the process crashes or exits.
  using CrashCallback = fit::function<void(zx_status_t exit_code)>;
  void MonitorForCrash(async_dispatcher_t* dispatcher,
                       CrashCallback callback);

 private:
  std::string name_;
  zx::process process_;
  zx::vmar root_vmar_;

  // Address of init_module and cleanup_module in the process's address space.
  uintptr_t init_fn_addr_ = 0;
  uintptr_t exit_fn_addr_ = 0;

  // Tracks loaded module count for colocation groups.
  int loaded_module_count_ = 0;
};

// Creates and manages child processes for .ko modules.
class ProcessManager {
 public:
  explicit ProcessManager(async_dispatcher_t* dispatcher);
  ~ProcessManager();

  // Create a new child process with the given name.
  // Returns nullptr on failure.
  std::unique_ptr<ManagedProcess> CreateProcess(const std::string& name);

 private:
  async_dispatcher_t* dispatcher_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_RUNNER_PROCESS_MANAGER_H_
