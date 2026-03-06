// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/vmo_module_loader.h"

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <zircon/process.h>
#include <zircon/rights.h>
#include <zircon/types.h>

namespace driverhub {

void VmoAllocation::Release() {
  if (base != 0 && size > 0) {
    zx::vmar::root_self()->unmap(base, size);
    base = 0;
    size = 0;
  }
  vmo.reset();
}

zx_status_t AllocateModuleMemory(size_t alloc_size, VmoAllocation* out) {
  // Create a VMO for the module sections.
  zx::vmo vmo;
  zx_status_t status = zx::vmo::create(alloc_size, 0, &vmo);
  if (status != ZX_OK) {
    return status;
  }

  // Name the VMO for debugging visibility.
  vmo.set_property(ZX_PROP_NAME, "driverhub-module", 16);

  // Replace the VMO with one that has execute rights.
  // This requires the caller to have the VMEX resource.
  status = vmo.replace_as_executable(zx::resource(), &vmo);
  if (status != ZX_OK) {
    return status;
  }

  // Map the VMO into the process VMAR with RWX permissions.
  // Using root VMAR for simplicity; a dedicated sub-VMAR could provide
  // better isolation.
  zx_vaddr_t mapped_addr = 0;
  status = zx::vmar::root_self()->map(
      ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE,
      /* vmar_offset */ 0, vmo, /* vmo_offset */ 0, alloc_size, &mapped_addr);
  if (status != ZX_OK) {
    return status;
  }

  out->vmo = std::move(vmo);
  out->base = mapped_addr;
  out->size = alloc_size;
  return ZX_OK;
}

}  // namespace driverhub
