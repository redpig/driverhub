// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_VMO_MODULE_LOADER_H_
#define DRIVERHUB_SRC_FUCHSIA_VMO_MODULE_LOADER_H_

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

#include <cstddef>
#include <cstdint>

#include "src/loader/memory_allocator.h"

namespace driverhub {

// Fuchsia-specific memory allocator using VMOs for module sections.
//
// Allocates a single VMO mapped with RWX for loading and relocation, then
// callers use Protect() to tighten per-section permissions:
//   .text   → R+X
//   .data   → R+W
//   .rodata → R
// Requires the caller to have the VMEX resource for executable mappings.
class VmoAllocator : public MemoryAllocator {
 public:
  MemoryAllocation* Allocate(size_t size) override;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_FUCHSIA_VMO_MODULE_LOADER_H_
