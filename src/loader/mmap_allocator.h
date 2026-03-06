// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_LOADER_MMAP_ALLOCATOR_H_
#define DRIVERHUB_SRC_LOADER_MMAP_ALLOCATOR_H_

#include "src/loader/memory_allocator.h"

namespace driverhub {

// Host (Linux) memory allocator using mmap for module sections.
class MmapAllocator : public MemoryAllocator {
 public:
  MemoryAllocation* Allocate(size_t size) override;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_LOADER_MMAP_ALLOCATOR_H_
