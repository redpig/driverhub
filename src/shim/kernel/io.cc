// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/io.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "src/shim/kernel/device.h"

// I/O mapping and DMA shims.
//
// In the host userspace environment:
//   - ioremap allocates regular memory (no real MMIO).
//   - DMA allocations use posix_memalign for alignment.
//   - copy_from/to_user are plain memcpy.
//
// On Fuchsia:
//   - ioremap → obtain physical VMO from platform device, map into VMAR.
//   - DMA → use BTI (Bus Transaction Initiator) for pinned/contiguous memory.

extern "C" {

// --- I/O memory mapping ---

void* ioremap(unsigned long phys_addr, unsigned long size) {
  // In userspace shim, allocate a fake MMIO region.
  // Reads/writes go to this buffer. The bus driver would need to connect
  // this to a real FIDL-backed MMIO on Fuchsia.
  void* ptr = calloc(1, size);
  fprintf(stderr, "driverhub: ioremap(0x%lx, %lu) = %p (simulated)\n",
          phys_addr, size, ptr);
  return ptr;
}

void iounmap(void* addr) {
  fprintf(stderr, "driverhub: iounmap(%p)\n", addr);
  free(addr);
}

void* ioremap_wc(unsigned long phys_addr, unsigned long size) {
  return ioremap(phys_addr, size);
}

void* ioremap_nocache(unsigned long phys_addr, unsigned long size) {
  return ioremap(phys_addr, size);
}

void* devm_ioremap(struct device* dev, unsigned long phys_addr,
                   unsigned long size) {
  (void)dev;  // TODO: Track for automatic cleanup.
  return ioremap(phys_addr, size);
}

void* devm_ioremap_resource(struct device* dev, const struct resource* res) {
  if (!res) return (void*)(-1L);  // ERR_PTR(-EINVAL)
  return devm_ioremap(dev, res->start, res->end - res->start + 1);
}

// --- MMIO read/write ---

uint8_t ioread8(const void* addr) {
  return *static_cast<const volatile uint8_t*>(addr);
}

uint16_t ioread16(const void* addr) {
  return *static_cast<const volatile uint16_t*>(addr);
}

uint32_t ioread32(const void* addr) {
  return *static_cast<const volatile uint32_t*>(addr);
}

void iowrite8(uint8_t val, void* addr) {
  *static_cast<volatile uint8_t*>(addr) = val;
}

void iowrite16(uint16_t val, void* addr) {
  *static_cast<volatile uint16_t*>(addr) = val;
}

void iowrite32(uint32_t val, void* addr) {
  *static_cast<volatile uint32_t*>(addr) = val;
}

// --- DMA ---

void* dma_alloc_coherent(struct device* dev, size_t size,
                         dma_addr_t* dma_handle, unsigned int flags) {
  (void)dev;
  (void)flags;
  // Allocate page-aligned memory. In a real Fuchsia implementation, this
  // would use zx_vmo_create_contiguous + zx_bti_pin.
  void* ptr = nullptr;
  if (posix_memalign(&ptr, 4096, size) != 0) {
    return nullptr;
  }
  memset(ptr, 0, size);
  if (dma_handle) {
    // In userspace, the "DMA address" is just the virtual address.
    *dma_handle = reinterpret_cast<dma_addr_t>(ptr);
  }
  fprintf(stderr, "driverhub: dma_alloc_coherent(%zu) = %p (simulated)\n",
          size, ptr);
  return ptr;
}

void dma_free_coherent(struct device* dev, size_t size,
                       void* cpu_addr, dma_addr_t dma_handle) {
  (void)dev;
  (void)size;
  (void)dma_handle;
  free(cpu_addr);
}

int dma_set_mask(struct device* dev, uint64_t mask) {
  (void)dev;
  (void)mask;
  return 0;  // Always succeeds in userspace.
}

int dma_set_coherent_mask(struct device* dev, uint64_t mask) {
  (void)dev;
  (void)mask;
  return 0;
}

int dma_set_mask_and_coherent(struct device* dev, uint64_t mask) {
  (void)dev;
  (void)mask;
  return 0;
}

// --- User-space copy ---

unsigned long copy_from_user(void* to, const void* from, unsigned long n) {
  memcpy(to, from, n);
  return 0;  // 0 = success (all bytes copied).
}

unsigned long copy_to_user(void* to, const void* from, unsigned long n) {
  memcpy(to, from, n);
  return 0;
}

}  // extern "C"
