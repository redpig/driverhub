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
//   - ioremap → zx_vmo_create_physical + zx_vmar_map via MMIO resource.
//   - inb/outb → real x86 I/O port instructions via ioport resource.
//   - DMA → use BTI (Bus Transaction Initiator) for pinned/contiguous memory.

#if defined(__Fuchsia__)
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include "src/fuchsia/resource_provider.h"
#endif

extern "C" {

// --- I/O memory mapping ---

void* ioremap(unsigned long phys_addr, unsigned long size) {
#if defined(__Fuchsia__)
  // Try real physical MMIO mapping using the MMIO resource.
  zx_handle_t mmio = dh_mmio_resource();
  if (mmio != ZX_HANDLE_INVALID) {
    zx_handle_t vmo;
    zx_status_t status = zx_vmo_create_physical(mmio, phys_addr, size, &vmo);
    if (status == ZX_OK) {
      zx_vaddr_t mapped_addr = 0;
      status = zx_vmar_map(zx_vmar_root_self(),
                           ZX_VM_PERM_READ | ZX_VM_PERM_WRITE,
                           0, vmo, 0, size, &mapped_addr);
      zx_handle_close(vmo);
      if (status == ZX_OK) {
        fprintf(stderr, "driverhub: ioremap(0x%lx, %lu) = %p (real MMIO)\n",
                phys_addr, size, (void*)mapped_addr);
        return reinterpret_cast<void*>(mapped_addr);
      }
    }
    fprintf(stderr, "driverhub: ioremap(0x%lx, %lu) physical map failed: %d\n",
            phys_addr, size, status);
  }
#endif
  // Fallback: allocate simulated MMIO region.
  void* ptr = calloc(1, size);
  fprintf(stderr, "driverhub: ioremap(0x%lx, %lu) = %p (simulated)\n",
          phys_addr, size, ptr);
  return ptr;
}

void iounmap(void* addr) {
  fprintf(stderr, "driverhub: iounmap(%p)\n", addr);
#if defined(__Fuchsia__)
  // TODO: Track real vs simulated mappings for proper cleanup.
  // For now, don't free - real MMIO mappings can't be freed with free().
#else
  free(addr);
#endif
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

// --- I/O region management ---
// These are stubs — in the Linux kernel, they manage the I/O port and MMIO
// resource tree. On Fuchsia, the equivalent is handled by zx_ioports_request
// and zx_vmo_create_physical.

struct resource* __request_region(struct resource* parent,
                                  unsigned long start, unsigned long n,
                                  const char* name, int flags) {
  (void)parent;
  (void)flags;
  static struct resource dummy_resource = {};
#if defined(__Fuchsia__) && defined(__x86_64__)
  // Request the I/O port range from the Fuchsia kernel.
  zx_status_t status = dh_ioports_request(static_cast<uint16_t>(start),
                                           static_cast<uint32_t>(n));
  if (status == ZX_OK) {
    fprintf(stderr,
            "driverhub: request_region(%s, 0x%lx, %lu) granted (hw)\n",
            name ? name : "?", start, n);
  } else {
    // The IoportResource capability isn't routed to this process.
    // In a real DFv2 deployment, the driver would have this capability
    // declared in its CML manifest. For now, proceed — the inb/outb
    // will either work (if IOPL is set) or fault.
    fprintf(stderr,
            "driverhub: request_region(%s, 0x%lx, %lu) no IOPL "
            "(need IoportResource capability, status=%d)\n",
            name ? name : "?", start, n, status);
  }
#else
  fprintf(stderr, "driverhub: request_region(%s, 0x%lx, %lu) (simulated)\n",
          name ? name : "?", start, n);
#endif
  return &dummy_resource;
}

void __release_region(struct resource* parent,
                      unsigned long start, unsigned long n) {
  (void)parent;
  fprintf(stderr, "driverhub: release_region(0x%lx, %lu)\n", start, n);
}

// --- x86 Port I/O ---
//
// On Fuchsia, the ioport resource must be acquired first via
// dh_resources_init() and dh_ioports_request(). Once the kernel grants
// access, userspace can execute in/out instructions directly.

#if defined(__Fuchsia__) && defined(__x86_64__)
// Real x86 I/O port instructions. Fuchsia grants userspace IOPL access
// for requested port ranges via zx_ioports_request().
uint8_t inb(uint16_t port) {
  uint8_t val;
  __asm__ volatile("inb %w1, %0" : "=a"(val) : "Nd"(port));
  return val;
}

uint16_t inw(uint16_t port) {
  uint16_t val;
  __asm__ volatile("inw %w1, %0" : "=a"(val) : "Nd"(port));
  return val;
}

uint32_t inl(uint16_t port) {
  uint32_t val;
  __asm__ volatile("inl %w1, %0" : "=a"(val) : "Nd"(port));
  return val;
}

void outb(uint8_t val, uint16_t port) {
  __asm__ volatile("outb %0, %w1" : : "a"(val), "Nd"(port));
}

void outw(uint16_t val, uint16_t port) {
  __asm__ volatile("outw %0, %w1" : : "a"(val), "Nd"(port));
}

void outl(uint32_t val, uint16_t port) {
  __asm__ volatile("outl %0, %w1" : : "a"(val), "Nd"(port));
}
#else
// Stub implementations for non-Fuchsia / non-x86 builds.
uint8_t inb(uint16_t port) { (void)port; return 0xFF; }
uint16_t inw(uint16_t port) { (void)port; return 0xFFFF; }
uint32_t inl(uint16_t port) { (void)port; return 0xFFFFFFFF; }
void outb(uint8_t val, uint16_t port) { (void)val; (void)port; }
void outw(uint16_t val, uint16_t port) { (void)val; (void)port; }
void outl(uint32_t val, uint16_t port) { (void)val; (void)port; }
#endif

// --- DMA streaming mappings ---
// In userspace, DMA addresses are simply the virtual addresses. On Fuchsia,
// these would go through zx_bti_pin for real IOMMU translation.

dma_addr_t dma_map_single(struct device* dev, void* ptr, size_t size,
                           enum dma_data_direction dir) {
  (void)dev;
  (void)size;
  (void)dir;
  return reinterpret_cast<dma_addr_t>(ptr);
}

void dma_unmap_single(struct device* dev, dma_addr_t addr, size_t size,
                       enum dma_data_direction dir) {
  (void)dev;
  (void)addr;
  (void)size;
  (void)dir;
}

dma_addr_t dma_map_page(struct device* dev, void* page,
                         unsigned long offset, size_t size,
                         enum dma_data_direction dir) {
  (void)dev;
  (void)size;
  (void)dir;
  return reinterpret_cast<dma_addr_t>(static_cast<char*>(page) + offset);
}

void dma_unmap_page(struct device* dev, dma_addr_t addr, size_t size,
                     enum dma_data_direction dir) {
  (void)dev;
  (void)addr;
  (void)size;
  (void)dir;
}

void dma_sync_single_for_cpu(struct device* dev, dma_addr_t addr,
                              size_t size, enum dma_data_direction dir) {
  (void)dev;
  (void)addr;
  (void)size;
  (void)dir;
  // No-op: userspace memory is always coherent.
}

void dma_sync_single_for_device(struct device* dev, dma_addr_t addr,
                                 size_t size, enum dma_data_direction dir) {
  (void)dev;
  (void)addr;
  (void)size;
  (void)dir;
}

int dma_mapping_error(struct device* dev, dma_addr_t addr) {
  (void)dev;
  return addr == 0 ? 1 : 0;
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
