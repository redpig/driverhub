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
// Uses VMOs mapped into the process VMAR with RWX permissions.
// Requires the caller to have the VMEX resource for executable mappings.
//
// TODO: Create separate VMOs per section type for proper permission
// separation (RX for .text, RW for .data, R for .rodata).
class VmoAllocator : public MemoryAllocator {
 public:
  MemoryAllocation* Allocate(size_t size) override;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_FUCHSIA_VMO_MODULE_LOADER_H_
