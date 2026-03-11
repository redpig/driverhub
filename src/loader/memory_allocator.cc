// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/memory_allocator.h"

namespace driverhub {

MemoryAllocation::~MemoryAllocation() = default;

void MemoryAllocation::Protect(size_t /*offset*/, size_t /*len*/,
                               uint32_t /*perms*/) {
  // Default no-op for host (mmap-based) allocations.
}

}  // namespace driverhub
