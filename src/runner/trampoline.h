// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_RUNNER_TRAMPOLINE_H_
#define DRIVERHUB_SRC_RUNNER_TRAMPOLINE_H_

// Architecture-specific trampoline stub generation for cross-process
// EXPORT_SYMBOL function calls.
//
// When a .ko module in Process A calls an EXPORT_SYMBOL function defined in
// a .ko module in Process B, the symbol proxy mechanism needs a trampoline:
//
// 1. The calling module's code jumps to the trampoline stub (which lives
//    in the caller's address space).
// 2. The trampoline saves register arguments to a shared VMO, invokes the
//    SymbolProxy FIDL protocol, and restores the return value.
//
// This file generates the architecture-specific machine code for these
// trampoline stubs. The stubs follow the platform C calling convention:
//   - AArch64: args in x0-x7, return in x0, LR in x30
//   - x86_64:  args in rdi/rsi/rdx/rcx/r8/r9, return in rax

#include <lib/zx/vmo.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace driverhub {

// Maximum number of register arguments supported by the trampoline.
// AArch64 supports 8 (x0-x7), x86_64 supports 6 (rdi-r9).
constexpr size_t kMaxTrampolineArgs = 8;

// Generate a trampoline stub for the current architecture.
//
// The trampoline:
// 1. Saves up to |num_args| register arguments into a buffer at
//    |shared_vmo_addr| (the VMO mapped into the caller's process).
// 2. Writes the argument count and sizes.
// 3. Calls a "dispatch" function at |dispatch_addr| which triggers the
//    FIDL SymbolProxy::Call RPC.
// 4. Reads the return value from the VMO and returns it in the
//    appropriate register (x0/rax).
//
// Returns the raw machine code bytes for the trampoline stub.
std::vector<uint8_t> GenerateTrampoline(uint64_t shared_vmo_addr,
                                         uint64_t dispatch_addr,
                                         size_t num_args);

// Generate an AArch64 trampoline stub.
std::vector<uint8_t> GenerateTrampolineAArch64(uint64_t shared_vmo_addr,
                                                uint64_t dispatch_addr,
                                                size_t num_args);

// Generate an x86_64 trampoline stub.
std::vector<uint8_t> GenerateTrampolineX86_64(uint64_t shared_vmo_addr,
                                               uint64_t dispatch_addr,
                                               size_t num_args);

// Write a trampoline stub into a VMO at the given offset.
// Returns the address where the trampoline was written (for use as the
// resolved symbol address in the caller's process).
zx_status_t WriteTrampolineToVmo(const zx::vmo& vmo,
                                  uint64_t offset,
                                  const std::vector<uint8_t>& code);

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_RUNNER_TRAMPOLINE_H_
