// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runner/process_manager.h"

#include <lib/async/cpp/wait.h>
#include <lib/zx/job.h>
#include <zircon/process.h>
#include <zircon/processargs.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>

#include <cstdio>

#include "src/runner/symbol_registry_server.h"

namespace driverhub {

// ---------------------------------------------------------------------------
// ManagedProcess
// ---------------------------------------------------------------------------

ManagedProcess::ManagedProcess(std::string name, zx::process process,
                               zx::vmar root_vmar)
    : name_(std::move(name)),
      process_(std::move(process)),
      root_vmar_(std::move(root_vmar)) {}

ManagedProcess::~ManagedProcess() {
  if (process_.is_valid()) {
    // Kill the process if still running.
    process_.kill();
  }
}

zx_status_t ManagedProcess::InjectShimLibrary() {
  // TODO(driverhub): Map libdriverhub_kmi.so into the child process.
  //
  // Implementation steps:
  // 1. Open the runner's own /pkg/lib/libdriverhub_kmi.so
  // 2. Create a VMO from the file
  // 3. Map the VMO into the child process's VMAR with appropriate
  //    permissions (R for .rodata, RW for .data/.bss, RX for .text)
  // 4. Set up the dynamic linker namespace so the child process can
  //    find the shared library
  //
  // For now, in the initial runner skeleton, we rely on the .ko loader
  // resolving symbols directly against the registry (same as the current
  // bus driver model). The shared library injection will be implemented
  // when we have the process launcher fully wired up.

  fprintf(stderr, "driverhub: process_manager: InjectShimLibrary(%s) [stub]\n",
          name_.c_str());
  return ZX_OK;
}

zx_status_t ManagedProcess::ReadModuleBinary(
    const fuchsia_component_runner::ComponentStartInfo& start_info,
    const std::string& ko_path,
    std::vector<uint8_t>* out) {
  // Find the /pkg namespace entry in the component's namespace.
  if (!start_info.ns().has_value()) {
    return ZX_ERR_NOT_FOUND;
  }

  for (const auto& entry : start_info.ns().value()) {
    if (!entry.path().has_value() || entry.path().value() != "/pkg") {
      continue;
    }

    if (!entry.directory().has_value()) {
      return ZX_ERR_NOT_FOUND;
    }

    // TODO(driverhub): Open ko_path within the /pkg directory handle
    // and read the .ko file contents into `out`.
    //
    // Implementation:
    // 1. Use fuchsia.io/Directory.Open to open the ko_path
    // 2. Read the file contents into a vector
    // 3. Return the raw ELF bytes
    //
    // For now, return NOT_SUPPORTED as this requires async FIDL I/O.
    fprintf(stderr,
            "driverhub: process_manager: ReadModuleBinary(%s) [stub]\n",
            ko_path.c_str());
    return ZX_ERR_NOT_SUPPORTED;
  }

  return ZX_ERR_NOT_FOUND;
}

zx_status_t ManagedProcess::LoadModule(const uint8_t* data, size_t size,
                                        SymbolRegistryServer& registry) {
  // TODO(driverhub): Load the .ko into this process's VMAR.
  //
  // This adapts the existing ModuleLoader to work cross-process:
  // 1. Parse ELF ET_REL headers
  // 2. Allocate memory in the child VMAR (not self)
  // 3. Copy section data into the child's address space
  // 4. Resolve symbols against:
  //    a. libdriverhub_kmi.so exports (shim functions)
  //    b. SymbolRegistryServer (intermodule EXPORT_SYMBOL)
  // 5. Apply ARM64 relocations
  // 6. Record init_fn_addr_ and exit_fn_addr_
  //
  // The key difference from the current loader is that memory allocation
  // targets the child process's VMAR, not the runner's own address space.

  fprintf(stderr,
          "driverhub: process_manager: LoadModule(%zu bytes) [stub]\n", size);
  loaded_module_count_++;
  return ZX_OK;
}

zx_status_t ManagedProcess::CallModuleInit() {
  // TODO(driverhub): Start a thread in the child process at init_fn_addr_.
  //
  // Implementation:
  // 1. Create a thread in the child process via zx_thread_create()
  // 2. Set up the thread's stack and entry point (init_fn_addr_)
  // 3. Start the thread via zx_process_start() or zx_thread_start()
  // 4. Wait for init to complete (via a synchronization mechanism)
  //
  // For colocated modules, the process is already running so we use
  // the existing dispatcher thread to call init sequentially.

  if (init_fn_addr_ == 0) {
    fprintf(stderr,
            "driverhub: process_manager: no init_fn for %s\n",
            name_.c_str());
    return ZX_ERR_NOT_FOUND;
  }

  fprintf(stderr,
          "driverhub: process_manager: CallModuleInit(%s) at 0x%lx\n",
          name_.c_str(), init_fn_addr_);
  return ZX_OK;
}

zx_status_t ManagedProcess::CallModuleExit() {
  if (exit_fn_addr_ == 0) {
    return ZX_OK;  // Module has no cleanup function.
  }

  fprintf(stderr,
          "driverhub: process_manager: CallModuleExit(%s) at 0x%lx\n",
          name_.c_str(), exit_fn_addr_);

  // TODO(driverhub): Dispatch exit function in the child process.
  return ZX_OK;
}

void ManagedProcess::MonitorForCrash(async_dispatcher_t* dispatcher,
                                      CrashCallback callback) {
  // TODO(driverhub): Set up async wait on ZX_PROCESS_TERMINATED.
  //
  // When the process terminates:
  // 1. Check the exit code via zx_object_get_info()
  // 2. If abnormal, invoke the callback with the exit code
  // 3. The runner will clean up SymbolRegistry entries and report
  //    the crash via ComponentController epitaph

  fprintf(stderr,
          "driverhub: process_manager: MonitorForCrash(%s) [stub]\n",
          name_.c_str());
}

// ---------------------------------------------------------------------------
// ProcessManager
// ---------------------------------------------------------------------------

ProcessManager::ProcessManager(async_dispatcher_t* dispatcher)
    : dispatcher_(dispatcher) {}

ProcessManager::~ProcessManager() = default;

std::unique_ptr<ManagedProcess> ProcessManager::CreateProcess(
    const std::string& name) {
  // Create a new Zircon process under the default job.
  zx::process child_process;
  zx::vmar child_vmar;

  std::string process_name = "driverhub:" + name;
  // Truncate to ZX_MAX_NAME_LEN if needed.
  if (process_name.size() >= ZX_MAX_NAME_LEN) {
    process_name.resize(ZX_MAX_NAME_LEN - 1);
  }

  zx_status_t status = zx::process::create(
      *zx::job::default_job(), process_name.c_str(), process_name.size(),
      0u, &child_process, &child_vmar);

  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: failed to create process %s: %s\n",
            name.c_str(), zx_status_get_string(status));
    return nullptr;
  }

  fprintf(stderr,
          "driverhub: process_manager: created process %s (koid=%lu)\n",
          process_name.c_str(), child_process.get());

  auto managed = std::make_unique<ManagedProcess>(
      name, std::move(child_process), std::move(child_vmar));

  // Set up crash monitoring.
  managed->MonitorForCrash(dispatcher_, [name](zx_status_t exit_code) {
    fprintf(stderr,
            "driverhub: process %s terminated with status %s\n",
            name.c_str(), zx_status_get_string(exit_code));
  });

  return managed;
}

}  // namespace driverhub
