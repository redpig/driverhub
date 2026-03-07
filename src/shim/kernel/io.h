// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_IO_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_IO_H_

// KMI shims for Linux kernel I/O mapping and DMA APIs.
//
// Provides: ioremap, iounmap, ioread/iowrite variants,
//           dma_alloc_coherent, dma_free_coherent,
//           copy_from_user, copy_to_user
//
// On Fuchsia:
//   - ioremap → zx_vmo_create_physical + zx_vmar_map via platform device FIDL
//   - dma_alloc_coherent → zx_bti_pin / zx_vmo_create_contiguous via BTI FIDL
//   - copy_from/to_user → memcpy (userspace context)

#include <stddef.h>
#include <stdint.h>

// __iomem is a Linux kernel sparse annotation; define as empty in userspace.
#ifndef __iomem
#define __iomem
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct device;
struct resource;

// --- I/O memory mapping ---

void *ioremap(unsigned long phys_addr, unsigned long size);
void iounmap(void *addr);

// Convenience variants.
void *ioremap_wc(unsigned long phys_addr, unsigned long size);
void *ioremap_nocache(unsigned long phys_addr, unsigned long size);

// Device-managed ioremap.
void *devm_ioremap(struct device *dev, unsigned long phys_addr,
                   unsigned long size);
void *devm_ioremap_resource(struct device *dev, const struct resource *res);

// --- MMIO read/write ---

uint8_t ioread8(const void *addr);
uint16_t ioread16(const void *addr);
uint32_t ioread32(const void *addr);

void iowrite8(uint8_t val, void *addr);
void iowrite16(uint16_t val, void *addr);
void iowrite32(uint32_t val, void *addr);

// readl/writel family (common in GKI modules).
#define readb(addr)       ioread8(addr)
#define readw(addr)       ioread16(addr)
#define readl(addr)       ioread32(addr)
#define writeb(val, addr) iowrite8(val, addr)
#define writew(val, addr) iowrite16(val, addr)
#define writel(val, addr) iowrite32(val, addr)

// --- DMA ---

typedef uint64_t dma_addr_t;

void *dma_alloc_coherent(struct device *dev, size_t size,
                         dma_addr_t *dma_handle, unsigned int flags);
void dma_free_coherent(struct device *dev, size_t size,
                       void *cpu_addr, dma_addr_t dma_handle);

int dma_set_mask(struct device *dev, uint64_t mask);
int dma_set_coherent_mask(struct device *dev, uint64_t mask);
int dma_set_mask_and_coherent(struct device *dev, uint64_t mask);

// --- x86 Port I/O ---

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);
void outb(uint8_t val, uint16_t port);
void outw(uint16_t val, uint16_t port);
void outl(uint32_t val, uint16_t port);

// --- I/O region management (stubs for userspace) ---
struct resource *__request_region(struct resource *parent,
                                  unsigned long start, unsigned long n,
                                  const char *name, int flags);
void __release_region(struct resource *parent,
                      unsigned long start, unsigned long n);

// --- User-space copy (no-op in userspace context) ---

unsigned long copy_from_user(void *to, const void *from, unsigned long n);
unsigned long copy_to_user(void *to, const void *from, unsigned long n);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_IO_H_
