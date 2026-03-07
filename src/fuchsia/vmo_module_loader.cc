// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/vmo_module_loader.h"

#include <cstdio>
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <zircon/process.h>
#include <zircon/rights.h>
#include <zircon/types.h>

namespace driverhub {

namespace {

struct VmoAllocation : public MemoryAllocation {
  zx::vmo vmo;

  void Release() override {
    if (base != nullptr && size > 0) {
      zx::vmar::root_self()->unmap(reinterpret_cast<uintptr_t>(base), size);
      base = nullptr;
      size = 0;
    }
    vmo.reset();
  }

  ~VmoAllocation() override { Release(); }
};

}  // namespace

MemoryAllocation* VmoAllocator::Allocate(size_t alloc_size) {
  // Create a VMO for the module sections.
  zx::vmo vmo;
  zx_status_t status = zx::vmo::create(alloc_size, 0, &vmo);
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: VMO create failed: %d\n", status);
    return nullptr;
  }

  // Name the VMO for debugging visibility.
  vmo.set_property(ZX_PROP_NAME, "driverhub-module", 16);

  // Replace the VMO with one that has execute rights.
  // This requires the caller to have the VMEX resource.
  status = vmo.replace_as_executable(zx::resource(), &vmo);
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: VMO replace_as_executable failed: %d\n",
            status);
    return nullptr;
  }

  // Map the VMO into the low 4GB of the address space.
  //
  // Linux .ko modules compiled with -mcmodel=small (the default) use
  // R_X86_64_32 / R_X86_64_32S relocations that require absolute addresses
  // to fit in 32 bits. On Fuchsia PIE processes, the default VMAR allocation
  // places mappings at high addresses (>4GB), causing 32-bit truncation.
  //
  // We use ZX_VM_OFFSET_IS_UPPER_LIMIT to request placement below 4GB,
  // similar to how Fuchsia's restricted_machine library handles low-address
  // code placement.
  zx_vaddr_t mapped_addr = 0;

  // Get root VMAR info to compute the offset for the upper limit.
  zx_info_vmar_t vmar_info;
  status = zx::vmar::root_self()->get_info(
      ZX_INFO_VMAR, &vmar_info, sizeof(vmar_info), nullptr, nullptr);
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: VMAR get_info failed: %d\n", status);
    return nullptr;
  }

  // Compute the offset within the root VMAR that corresponds to the 4GB mark.
  // ZX_VM_OFFSET_IS_UPPER_LIMIT tells the kernel to place the mapping at an
  // address below (vmar_base + offset).
  // R_X86_64_32S sign-extends: addresses must be < 2GB for mcmodel=small.
  constexpr uint64_t kLow2GB = 0x80000000ULL;
  bool try_low = (kLow2GB > vmar_info.base + alloc_size);

  if (try_low) {
    uint64_t upper_offset = kLow2GB - vmar_info.base;
    status = zx::vmar::root_self()->map(
        ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE |
            ZX_VM_OFFSET_IS_UPPER_LIMIT,
        upper_offset, vmo, 0, alloc_size, &mapped_addr);
    if (status != ZX_OK) {
      fprintf(stderr,
              "driverhub: low-address map failed: %d (base=%#lx, "
              "upper_offset=%#lx), falling back\n",
              status, vmar_info.base, upper_offset);
      try_low = false;
    }
  }

  if (!try_low) {
    // Fall back to default allocation (may be >4GB).
    status = zx::vmar::root_self()->map(
        ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE,
        0, vmo, 0, alloc_size, &mapped_addr);
    if (status != ZX_OK) {
      fprintf(stderr, "driverhub: VMAR map failed: %d\n", status);
      return nullptr;
    }
  }

  fprintf(stderr, "driverhub: module mapped at %#lx (%zu bytes)\n",
          mapped_addr, alloc_size);

  auto* alloc = new VmoAllocation();
  alloc->vmo = std::move(vmo);
  alloc->base = reinterpret_cast<uint8_t*>(mapped_addr);
  alloc->size = alloc_size;
  return alloc;
}

}  // namespace driverhub
