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

  // Map the VMO into the process address space.
  //
  // On x86_64, Linux .ko modules compiled with -mcmodel=small (the default)
  // use R_X86_64_32/R_X86_64_32S relocations requiring addresses < 2GB.
  // We use ZX_VM_OFFSET_IS_UPPER_LIMIT to request low-address placement.
  //
  // On aarch64, GKI modules use -mcmodel=large and position-independent
  // relocations (ADRP/ADD), so any address is fine.
  zx_vaddr_t mapped_addr = 0;

#if defined(__x86_64__)
  // Get root VMAR info to compute the offset for the upper limit.
  zx_info_vmar_t vmar_info;
  status = zx::vmar::root_self()->get_info(
      ZX_INFO_VMAR, &vmar_info, sizeof(vmar_info), nullptr, nullptr);
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: VMAR get_info failed: %d\n", status);
    return nullptr;
  }

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

  if (!try_low)
#endif  // __x86_64__
  {
    // Default allocation — no address constraint.
    // On aarch64 this is the only path; on x86_64 it's the fallback.
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
