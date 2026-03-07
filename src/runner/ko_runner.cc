// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runner/ko_runner.h"

#include <lib/async/cpp/task.h>
#include <zircon/status.h>

#include <cstdio>

namespace driverhub {

KoRunner::KoRunner(async_dispatcher_t* dispatcher,
                   SymbolRegistryServer& symbol_registry)
    : dispatcher_(dispatcher),
      symbol_registry_(symbol_registry),
      process_manager_(dispatcher) {}

KoRunner::~KoRunner() = default;

void KoRunner::Start(StartRequest& request, StartCompleter::Sync& completer) {
  auto& start_info = request.start_info();
  auto& controller = request.controller();

  // Extract the component URL for identification.
  std::string url;
  if (start_info.resolved_url().has_value()) {
    url = start_info.resolved_url().value();
  }

  // Parse the program block to get module metadata.
  ModuleComponentInfo module_info;
  if (!start_info.program().has_value()) {
    fprintf(stderr, "driverhub: ko_runner: Start() missing program block\n");
    controller.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  zx_status_t status = ParseProgramInfo(start_info.program().value(),
                                         &module_info);
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: ko_runner: failed to parse program info: %s\n",
            zx_status_get_string(status));
    controller.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  fprintf(stderr, "driverhub: ko_runner: Start(%s) ko=%s group=%s\n",
          url.c_str(), module_info.ko_binary.c_str(),
          module_info.colocation_group.empty()
              ? "<isolated>"
              : module_info.colocation_group.c_str());

  // Determine the target process.
  ManagedProcess* target = nullptr;

  if (!module_info.colocation_group.empty()) {
    target = FindOrCreateGroupProcess(module_info.colocation_group);
    if (!target) {
      fprintf(stderr,
              "driverhub: ko_runner: failed to create group process for %s\n",
              module_info.colocation_group.c_str());
      controller.Close(ZX_ERR_NO_RESOURCES);
      return;
    }
  } else {
    // Isolated module: create a new process.
    auto process = process_manager_.CreateProcess(module_info.ko_binary);
    if (!process) {
      fprintf(stderr,
              "driverhub: ko_runner: failed to create process for %s\n",
              module_info.ko_binary.c_str());
      controller.Close(ZX_ERR_NO_RESOURCES);
      return;
    }
    target = process.get();

    // Track the process. For isolated modules, use the URL as key.
    auto running = std::make_unique<RunningModule>();
    running->name = module_info.ko_binary;
    running->process = std::move(process->process_handle());
    running->controller = std::move(controller);
    running_modules_[url] = std::move(running);
  }

  // Load the .ko into the target process.
  status = StartModuleInProcess(*target, module_info, start_info);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: ko_runner: failed to start module %s: %s\n",
            module_info.ko_binary.c_str(), zx_status_get_string(status));
    // Close the controller with an error epitaph.
    if (auto it = running_modules_.find(url); it != running_modules_.end()) {
      it->second->controller.Close(ZX_ERR_INTERNAL);
      running_modules_.erase(it);
    }
    return;
  }

  fprintf(stderr, "driverhub: ko_runner: module %s started successfully\n",
          module_info.ko_binary.c_str());
}

// static
zx_status_t KoRunner::ParseProgramInfo(
    const fuchsia_data::Dictionary& program,
    ModuleComponentInfo* out) {
  if (!program.entries().has_value()) {
    return ZX_ERR_INVALID_ARGS;
  }

  for (const auto& entry : program.entries().value()) {
    if (!entry.key().has_value() || !entry.value().has_value()) {
      continue;
    }

    const auto& key = entry.key().value();
    const auto& value = entry.value().value();

    // Values in fuchsia.data.Dictionary are DictionaryValue, which is a
    // union of str, str_vec, or obj_vec.
    if (!value.str().has_value()) {
      // Handle str_vec for group_modules.
      if (key == "group_modules" && value.str_vec().has_value()) {
        for (const auto& s : value.str_vec().value()) {
          out->group_modules.push_back(std::string(s));
        }
      }
      continue;
    }

    const auto& str_val = value.str().value();
    if (key == "binary") {
      out->ko_binary = std::string(str_val);
    } else if (key == "colocation_group") {
      out->colocation_group = std::string(str_val);
    }
  }

  if (out->ko_binary.empty()) {
    return ZX_ERR_INVALID_ARGS;
  }

  return ZX_OK;
}

ManagedProcess* KoRunner::FindOrCreateGroupProcess(const std::string& group) {
  auto it = colocation_groups_.find(group);
  if (it != colocation_groups_.end()) {
    return it->second.get();
  }

  auto process = process_manager_.CreateProcess(group);
  if (!process) {
    return nullptr;
  }

  auto* ptr = process.get();
  colocation_groups_[group] = std::move(process);
  return ptr;
}

zx_status_t KoRunner::StartModuleInProcess(
    ManagedProcess& process,
    const ModuleComponentInfo& info,
    const fuchsia_component_runner::ComponentStartInfo& start_info) {
  // Step 1: Inject libdriverhub_kmi.so into the process.
  zx_status_t status = process.InjectShimLibrary();
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: failed to inject shim library: %s\n",
            zx_status_get_string(status));
    return status;
  }

  // Step 2: Read the .ko binary from the component namespace.
  // The .ko is at /pkg/<ko_binary> in the component's namespace.
  std::vector<uint8_t> ko_data;
  status = process.ReadModuleBinary(start_info, info.ko_binary, &ko_data);
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: failed to read .ko binary %s: %s\n",
            info.ko_binary.c_str(), zx_status_get_string(status));
    return status;
  }

  // Step 3: Load the .ko into the process VMAR via elfldltl.
  status = process.LoadModule(ko_data.data(), ko_data.size(),
                              symbol_registry_);
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: failed to load module %s: %s\n",
            info.ko_binary.c_str(), zx_status_get_string(status));
    return status;
  }

  // Step 4: Call module_init() in the target process.
  status = process.CallModuleInit();
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: module_init() failed for %s: %s\n",
            info.ko_binary.c_str(), zx_status_get_string(status));
    return status;
  }

  return ZX_OK;
}

}  // namespace driverhub
