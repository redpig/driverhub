// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_TYPES_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_TYPES_H_

// Common type definitions used throughout DriverHub.
//
// On Fuchsia, zx_status_t is provided by <zircon/types.h>.
// For standalone (host) builds, we define it here as int32_t.

#include <stdint.h>

#ifdef __Fuchsia__
#include <zircon/types.h>
#else
typedef int32_t zx_status_t;
#define ZX_OK 0
#define ZX_ERR_INTERNAL (-1)
#define ZX_ERR_NOT_FOUND (-3)
#define ZX_ERR_IO (-5)
#define ZX_ERR_ALREADY_BOUND (-26)
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_TYPES_H_
