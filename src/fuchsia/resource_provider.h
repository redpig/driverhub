// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_RESOURCE_PROVIDER_H_
#define DRIVERHUB_SRC_FUCHSIA_RESOURCE_PROVIDER_H_

// Acquires Fuchsia kernel resources (I/O port, MMIO, VMEX) via FIDL
// protocols from the component namespace, using raw channel calls
// (no generated bindings required).
//
// These resources are needed for real hardware access:
//   - IoportResource: for x86 I/O port access (inb/outb)
//   - MmioResource:   for physical MMIO mapping (ioremap)
//   - VmexResource:   for executable VMO creation (module loading)

#include <zircon/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize Fuchsia resource handles. Call once at startup.
// Returns ZX_OK on success. Individual resource failures are non-fatal
// (the corresponding feature just won't work).
zx_status_t dh_resources_init(void);

// Get the I/O port resource handle (for zx_ioports_request).
// Returns ZX_HANDLE_INVALID if not available.
zx_handle_t dh_ioport_resource(void);

// Get the MMIO resource handle (for zx_vmo_create_physical).
// Returns ZX_HANDLE_INVALID if not available.
zx_handle_t dh_mmio_resource(void);

// Get the VMEX resource handle (for zx_vmo_replace_as_executable).
// Returns ZX_HANDLE_INVALID if not available.
zx_handle_t dh_vmex_resource(void);

// Request access to a range of x86 I/O ports.
// Uses the ioport resource obtained at init.
zx_status_t dh_ioports_request(uint16_t io_addr, uint32_t len);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_FUCHSIA_RESOURCE_PROVIDER_H_
