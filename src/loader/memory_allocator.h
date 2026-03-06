// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_LOADER_MEMORY_ALLOCATOR_H_
#define DRIVERHUB_SRC_LOADER_MEMORY_ALLOCATOR_H_

#include <cstddef>
#include <cstdint>

namespace driverhub {

// Represents a block of executable memory allocated for module sections.
// The allocator owns the memory; call Release() or let the destructor clean up.
struct MemoryAllocation {
  uint8_t* base = nullptr;
  size_t size = 0;

  virtual ~MemoryAllocation();
  virtual void Release() = 0;

  MemoryAllocation() = default;
  MemoryAllocation(const MemoryAllocation&) = delete;
  MemoryAllocation& operator=(const MemoryAllocation&) = delete;
};

// Abstract interface for allocating executable memory for loaded modules.
//
// On host (Linux): backed by mmap with PROT_READ|PROT_WRITE|PROT_EXEC.
// On Fuchsia: backed by VMOs mapped into the process VMAR.
class MemoryAllocator {
 public:
  virtual ~MemoryAllocator() = default;

  // Allocate `size` bytes of RWX memory for module sections.
  // Returns nullptr on failure.
  virtual MemoryAllocation* Allocate(size_t size) = 0;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_LOADER_MEMORY_ALLOCATOR_H_
