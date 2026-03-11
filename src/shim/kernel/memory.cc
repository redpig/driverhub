// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/memory.h"

#include <cstdlib>
#include <cstring>

#ifdef __Fuchsia__
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <zircon/process.h>

#include <mutex>
#include <unordered_map>

// Track vmalloc allocations so vfree can unmap the correct size.
namespace {
struct VmallocEntry {
  zx_vaddr_t addr;
  size_t size;
};
std::mutex g_vmalloc_mu;
std::unordered_map<uintptr_t, VmallocEntry> g_vmalloc_map;
}  // namespace
#endif  // __Fuchsia__

// KMI memory shims: map Linux kernel allocators to userspace equivalents.
// On Fuchsia, vmalloc/vfree use zx_vmar_map/zx_vmar_unmap for page-aligned
// allocation. On host (POSIX), they use the C library allocator.

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
#ifdef __Fuchsia__
  if (size == 0) return nullptr;
  // Round up to page size.
  size_t alloc_size = (size + 4095) & ~4095UL;
  zx::vmo vmo;
  if (zx::vmo::create(alloc_size, 0, &vmo) != ZX_OK) return nullptr;
  zx_vaddr_t addr = 0;
  if (zx::vmar::root_self()->map(ZX_VM_PERM_READ | ZX_VM_PERM_WRITE,
                                  0, vmo, 0, alloc_size, &addr) != ZX_OK) {
    return nullptr;
  }
  {
    std::lock_guard<std::mutex> lock(g_vmalloc_mu);
    g_vmalloc_map[addr] = {addr, alloc_size};
  }
  return reinterpret_cast<void*>(addr);
#else
  return malloc(size);
#endif
}

void* vzalloc(unsigned long size) {
#ifdef __Fuchsia__
  // zx_vmo_create returns zero-filled pages, so vmalloc is sufficient.
  return vmalloc(size);
#else
  return calloc(1, size);
#endif
}

void vfree(const void* addr) {
  if (!addr) return;
#ifdef __Fuchsia__
  uintptr_t key = reinterpret_cast<uintptr_t>(addr);
  size_t unmap_size = 0;
  {
    std::lock_guard<std::mutex> lock(g_vmalloc_mu);
    auto it = g_vmalloc_map.find(key);
    if (it != g_vmalloc_map.end()) {
      unmap_size = it->second.size;
      g_vmalloc_map.erase(it);
    }
  }
  if (unmap_size > 0) {
    zx::vmar::root_self()->unmap(key, unmap_size);
  }
#else
  free(const_cast<void*>(addr));
#endif
}

// kmem_cache — simplified slab allocator backed by malloc.
// The "cache" just remembers the object size for allocation.

struct kmem_cache {
  size_t object_size;
  char name[32];
};

struct kmem_cache* kmem_cache_create(const char* name, unsigned int size,
                                      unsigned int align, unsigned long flags,
                                      void (*ctor)(void*)) {
  (void)align; (void)flags; (void)ctor;
  auto* cache = static_cast<struct kmem_cache*>(
      calloc(1, sizeof(struct kmem_cache)));
  if (!cache) return nullptr;
  cache->object_size = size;
  if (name) {
    strncpy(cache->name, name, sizeof(cache->name) - 1);
  }
  return cache;
}

struct kmem_cache* kmem_cache_create_usercopy(const char* name,
                                               unsigned int size,
                                               unsigned int align,
                                               unsigned long flags,
                                               unsigned int useroffset,
                                               unsigned int usersize,
                                               void (*ctor)(void*)) {
  (void)useroffset; (void)usersize;
  return kmem_cache_create(name, size, align, flags, ctor);
}

void kmem_cache_destroy(struct kmem_cache* cache) {
  free(cache);
}

void* kmem_cache_alloc(struct kmem_cache* cache, unsigned int flags) {
  (void)flags;
  if (!cache) return nullptr;
  return calloc(1, cache->object_size);
}

void kmem_cache_free(struct kmem_cache* cache, void* obj) {
  (void)cache;
  free(obj);
}

}  // extern "C"
