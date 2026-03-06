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

  // Map the VMO into the process VMAR with RWX permissions.
  zx_vaddr_t mapped_addr = 0;
  status = zx::vmar::root_self()->map(
      ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE,
      /* vmar_offset */ 0, vmo, /* vmo_offset */ 0, alloc_size, &mapped_addr);
  if (status != ZX_OK) {
    fprintf(stderr, "driverhub: VMAR map failed: %d\n", status);
    return nullptr;
  }

  auto* alloc = new VmoAllocation();
  alloc->vmo = std::move(vmo);
  alloc->base = reinterpret_cast<uint8_t*>(mapped_addr);
  alloc->size = alloc_size;
  return alloc;
}

}  // namespace driverhub
