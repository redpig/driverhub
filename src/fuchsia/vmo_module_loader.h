// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_VMO_MODULE_LOADER_H_
#define DRIVERHUB_SRC_FUCHSIA_VMO_MODULE_LOADER_H_

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

#include <cstddef>
#include <cstdint>

#include "src/loader/module_loader.h"

namespace driverhub {

// Fuchsia-specific module memory allocator.
//
// On Fuchsia, we use VMOs (Virtual Memory Objects) for module section memory
// instead of mmap. This provides:
//   - Proper permission separation (RX for .text, RW for .data, R for .rodata)
//   - Integration with the Fuchsia memory model
//   - Ability to use the process VMAR for mapping
//
// This is used by the module loader when running on Fuchsia. The host build
// continues to use mmap directly.
struct VmoAllocation {
  zx::vmo vmo;           // Backing VMO for the allocation.
  uintptr_t base;        // Mapped base address in the process VMAR.
  size_t size;           // Total allocation size.

  // Unmap and release the allocation.
  void Release();
};

// Allocate RWX memory for module sections using a VMO.
//
// Creates a VMO of the requested size, maps it into the process VMAR with
// read/write/execute permissions, and returns the allocation.
//
// In a production deployment, this should create separate VMOs for:
//   - .text (RX) — code sections
//   - .data/.bss (RW) — writable data sections
//   - .rodata (R) — read-only data sections
//
// For now, a single RWX VMO is used for simplicity (matching the host mmap
// behavior). Permission hardening is tracked as a follow-up.
zx_status_t AllocateModuleMemory(size_t size, VmoAllocation* out);

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_FUCHSIA_VMO_MODULE_LOADER_H_
