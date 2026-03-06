// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/mmap_allocator.h"

#include <cstdio>
#include <sys/mman.h>

namespace driverhub {

namespace {

struct MmapAllocation : public MemoryAllocation {
  void Release() override {
    if (base != nullptr && size > 0) {
      munmap(base, size);
      base = nullptr;
      size = 0;
    }
  }

  ~MmapAllocation() override { Release(); }
};

}  // namespace

MemoryAllocation* MmapAllocator::Allocate(size_t alloc_size) {
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(__x86_64__) && defined(MAP_32BIT)
  // Use MAP_32BIT on x86_64 to keep allocations in the low 2GB, which is
  // required for R_X86_64_32 relocations in kernel modules.
  flags |= MAP_32BIT;
#endif

  void* ptr = mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   flags, -1, 0);
  if (ptr == MAP_FAILED) {
    fprintf(stderr, "driverhub: mmap failed for %zu bytes\n", alloc_size);
    return nullptr;
  }

  auto* alloc = new MmapAllocation();
  alloc->base = static_cast<uint8_t*>(ptr);
  alloc->size = alloc_size;
  return alloc;
}

}  // namespace driverhub
