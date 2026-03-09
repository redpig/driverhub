// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_MEMORY_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_MEMORY_H_

// KMI shims for Linux kernel memory allocation APIs.
//
// Provides: kmalloc, kfree, kzalloc, krealloc, kcalloc,
//           vmalloc, vfree, vzalloc
//
// kmalloc/kfree map to userspace allocator.
// vmalloc/vfree map to zx_vmar_map / zx_vmar_unmap.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// GFP flags (subset used by GKI modules). Ignored in userspace shim but
// accepted for API compatibility.
typedef unsigned int gfp_t;

#define GFP_KERNEL  0x0u
#define GFP_ATOMIC  0x1u
#define GFP_DMA     0x2u

void* kmalloc(size_t size, gfp_t flags);
void* kzalloc(size_t size, gfp_t flags);
void* krealloc(const void* p, size_t new_size, gfp_t flags);
void* kcalloc(size_t n, size_t size, gfp_t flags);
void kfree(const void* p);

// Slab cache allocator — simplified kmem_cache backed by malloc.
struct kmem_cache;

struct kmem_cache* kmem_cache_create(const char* name, unsigned int size,
                                      unsigned int align, unsigned long flags,
                                      void (*ctor)(void*));
struct kmem_cache* kmem_cache_create_usercopy(const char* name,
                                               unsigned int size,
                                               unsigned int align,
                                               unsigned long flags,
                                               unsigned int useroffset,
                                               unsigned int usersize,
                                               void (*ctor)(void*));
void kmem_cache_destroy(struct kmem_cache* cache);
void* kmem_cache_alloc(struct kmem_cache* cache, unsigned int flags);
void kmem_cache_free(struct kmem_cache* cache, void* obj);

void* vmalloc(unsigned long size);
void* vzalloc(unsigned long size);
void vfree(const void* addr);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_MEMORY_H_
