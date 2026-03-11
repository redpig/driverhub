// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_RUNNER_PROCESS_MANAGER_H_
#define DRIVERHUB_SRC_RUNNER_PROCESS_MANAGER_H_

#include <fidl/fuchsia.component.runner/cpp/fidl.h>
#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>
#include <lib/zx/process.h>
#include <lib/zx/thread.h>
#include <lib/zx/vmar.h>
#include <lib/zx/job.h>
#include <lib/zx/vmo.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace driverhub {

class SymbolRegistryServer;
struct ModuleProcessInfo;
using ModuleProcessLookup =
    std::function<bool(const std::string& module_name, ModuleProcessInfo* out)>;

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
  // Parses the shared library's ELF segments, maps them into the child
  // VMAR, applies RELATIVE relocations, and extracts exported symbols.
  zx_status_t InjectShimLibrary();

  // Read the .ko binary from the component's namespace.
  zx_status_t ReadModuleBinary(
      const fuchsia_component_runner::ComponentStartInfo& start_info,
      const std::string& ko_path,
      std::vector<uint8_t>* out);

  // Load a .ko module into this process's VMAR. Parses ELF ET_REL,
  // allocates sections via VMO, resolves symbols against the shim library
  // and the FIDL SymbolRegistry, and applies AArch64/x86_64 relocations.
  zx_status_t LoadModule(const uint8_t* data, size_t size,
                         SymbolRegistryServer& registry);

  // Call the loaded module's init_module() entry point by creating a
  // thread in the child process and waiting for it to complete.
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

  // Address of init_module and cleanup_module in the child's address space.
  uintptr_t init_fn_addr_ = 0;
  uintptr_t exit_fn_addr_ = 0;

  // Tracks loaded module count for colocation groups.
  int loaded_module_count_ = 0;

  // Whether the process has been started (first thread created).
  bool process_started_ = false;

  // Shim library base address in the child process's address space.
  zx_vaddr_t shim_base_ = 0;

  // Maps KMI symbol names to virtual addresses in the child process.
  // Built by parsing the shim library's .dynsym section.
  std::unordered_map<std::string, uint64_t> shim_symbols_;

  // VMOs and mappings backing memory in the child process.
  // Kept alive so the child's memory remains valid.
  struct ChildMapping {
    zx::vmo vmo;
    zx_vaddr_t addr = 0;
    size_t size = 0;
  };
  std::vector<ChildMapping> child_mappings_;

  // Trampoline state for cross-process EXPORT_SYMBOL function calls.
  // A single executable VMO holds all trampoline stubs for this process.
  zx::vmo trampoline_vmo_;
  zx_vaddr_t trampoline_base_ = 0;
  uint64_t trampoline_offset_ = 0;

  // Async wait for process termination monitoring.
  std::unique_ptr<async::Wait> process_wait_;
  CrashCallback crash_callback_;
};

// Creates and manages child processes for .ko modules.
class ProcessManager {
 public:
  // |job| is the job under which child processes are created. In the
  // package-based world this is typically zx::job::default_job(); in
  // bootfs the runner receives a scoped job via capability routing.
  // If |job| is invalid, falls back to zx::job::default_job().
  ProcessManager(async_dispatcher_t* dispatcher, zx::unowned_job job);

  // Convenience overload — uses zx::job::default_job().
  explicit ProcessManager(async_dispatcher_t* dispatcher);
  ~ProcessManager();

  // Create a new child process with the given name.
  // Returns nullptr on failure.
  std::unique_ptr<ManagedProcess> CreateProcess(const std::string& name);

 private:
  async_dispatcher_t* dispatcher_;
  zx::unowned_job job_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_RUNNER_PROCESS_MANAGER_H_
