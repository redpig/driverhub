// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/memory.h"

#include <cstdlib>
#include <cstring>

// KMI memory shims: map Linux kernel allocators to userspace equivalents.
// In Fuchsia, vmalloc/vfree would map to zx_vmar_map/zx_vmar_unmap. For now,
// all allocators use the C library allocator since modules run in userspace.

extern "C" {

void* kmalloc(size_t size, gfp_t flags) {
  (void)flags;  // GFP flags are meaningless in userspace.
  return malloc(size);
}

void* kzalloc(size_t size, gfp_t flags) {
  (void)flags;
  return calloc(1, size);
}

void* krealloc(const void* p, size_t new_size, gfp_t flags) {
  (void)flags;
  // Linux krealloc with NULL acts like kmalloc; with size 0 acts like kfree.
  if (new_size == 0) {
    free(const_cast<void*>(p));
    return NULL;
  }
  return realloc(const_cast<void*>(p), new_size);
}

void* kcalloc(size_t n, size_t size, gfp_t flags) {
  (void)flags;
  return calloc(n, size);
}

void kfree(const void* p) {
  free(const_cast<void*>(p));
}

void* vmalloc(unsigned long size) {
  // TODO: Map to zx_vmar_map for page-aligned allocation on Fuchsia.
  return malloc(size);
}

void* vzalloc(unsigned long size) {
  // TODO: Map to zx_vmar_map + zero fill on Fuchsia.
  return calloc(1, size);
}

void vfree(const void* addr) {
  // TODO: Map to zx_vmar_unmap on Fuchsia.
  free(const_cast<void*>(addr));
}

}  // extern "C"
