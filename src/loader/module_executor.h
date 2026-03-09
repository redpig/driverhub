// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_LOADER_MODULE_EXECUTOR_H_
#define DRIVERHUB_SRC_LOADER_MODULE_EXECUTOR_H_

// Safe execution wrapper for GKI module init/exit functions.
//
// Modules are pre-compiled ARM64 code from the Linux kernel. When running
// them in userspace with shimmed KMI, any unimplemented or incorrectly
// shimmed function can cause a fault (SIGSEGV, SIGBUS, SIGILL).
//
// ModuleExecutor provides:
//   - Fault isolation: catches crashes in module code and reports the
//     faulting address instead of terminating the process.
//   - On host (Linux): uses POSIX signal handlers with sigsetjmp/siglongjmp.
//   - On Fuchsia: uses Zircon exception channels on a dedicated thread.
//
// Usage:
//   ModuleExecutor exec;
//   int ret = exec.CallInit(module->init_fn);
//   if (ret == MODULE_EXEC_FAULT) {
//     // Module crashed — fault address logged.
//   }

namespace driverhub {

// Return value indicating a fault occurred during module execution.
constexpr int MODULE_EXEC_FAULT = -0xDEAD;

// Execute a module's init_module function with fault protection.
// Returns init_fn's return value on success, MODULE_EXEC_FAULT on crash.
int SafeCallInit(int (*init_fn)());

// Execute a module's cleanup_module function with fault protection.
// Returns 0 on success, MODULE_EXEC_FAULT on crash.
int SafeCallExit(void (*exit_fn)());

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_LOADER_MODULE_EXECUTOR_H_
