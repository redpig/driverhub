// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/resource_provider.h"

#include <cstdio>
#include <cstring>
#include <lib/fdio/directory.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/types.h>

// Raw FIDL call helper to invoke a simple Get() method on a resource protocol.
// These protocols all have the same shape: no request body, response is a
// single resource handle.
//
// FIDL wire format (v2):
//   Request:  16-byte transaction header (txid, at_rest_flags, dynamic_flags,
//             magic, ordinal)
//   Response: 16-byte header + no inline body (handle is out-of-band)

namespace {

// FIDL transaction header (v2 wire format).
struct fidl_message_header {
  uint32_t txid;
  uint8_t at_rest_flags[2];
  uint8_t dynamic_flags;
  uint8_t magic_number;
  uint64_t ordinal;
};
static_assert(sizeof(fidl_message_header) == 16);

constexpr uint8_t kFidlWireFormatMagic = 0x01;

zx_handle_t g_ioport_resource = ZX_HANDLE_INVALID;
zx_handle_t g_mmio_resource = ZX_HANDLE_INVALID;
zx_handle_t g_vmex_resource = ZX_HANDLE_INVALID;

// Connect to a FIDL service by path and call Get() to obtain a resource handle.
zx_status_t AcquireResource(const char* service_path, uint64_t ordinal,
                             zx_handle_t* out_handle) {
  // Create a channel pair.
  zx_handle_t client, server;
  zx_status_t status = zx_channel_create(0, &client, &server);
  if (status != ZX_OK) return status;

  // Connect to the service.
  status = fdio_service_connect(service_path, server);
  if (status != ZX_OK) {
    zx_handle_close(client);
    return status;
  }

  // Build the FIDL request (just a header, no body).
  fidl_message_header request = {};
  request.txid = 1;
  request.at_rest_flags[0] = 0x02;  // v2 wire format
  request.dynamic_flags = 0;
  request.magic_number = kFidlWireFormatMagic;
  request.ordinal = ordinal;

  // FIDL v2 response layout for "-> (resource struct { handle })":
  //   16 bytes: transaction header
  //    4 bytes: handle present marker (0xFFFFFFFF)
  //    4 bytes: padding
  // Total: 24 bytes, plus 1 out-of-band handle.
  struct {
    fidl_message_header hdr;
    uint32_t handle_present;
    uint32_t padding;
  } response = {};
  zx_handle_t handle_buf = ZX_HANDLE_INVALID;
  uint32_t actual_bytes = 0, actual_handles = 0;

  zx_channel_call_args_t args = {};
  args.wr_bytes = &request;
  args.wr_num_bytes = sizeof(request);
  args.wr_handles = nullptr;
  args.wr_num_handles = 0;
  args.rd_bytes = &response;
  args.rd_num_bytes = sizeof(response);
  args.rd_handles = &handle_buf;
  args.rd_num_handles = 1;

  status = zx_channel_call(client, 0, ZX_TIME_INFINITE, &args,
                           &actual_bytes, &actual_handles);
  zx_handle_close(client);

  if (status != ZX_OK) return status;

  if (actual_handles >= 1 && handle_buf != ZX_HANDLE_INVALID) {
    *out_handle = handle_buf;
    return ZX_OK;
  }

  return ZX_ERR_INTERNAL;
}

}  // namespace

// FIDL ordinals for resource protocols (SHA-256 hash of method name).
// Computed from: SHA-256("<library>/<Protocol>.<Method>") & 0x7FFFFFFFFFFFFFFF
constexpr uint64_t kIoportResourceGetOrdinal = 0x4db20876b537c52bULL;
constexpr uint64_t kMmioResourceGetOrdinal = 0x66747b9c5a6dfe7bULL;
constexpr uint64_t kVmexResourceGetOrdinal = 0x33db32deed650699ULL;
constexpr uint64_t kDebugResourceGetOrdinal = 0x1d79d77ea12a6474ULL;
constexpr uint64_t kInfoResourceGetOrdinal = 0x4e637e3aee3e0109ULL;

extern "C" {

zx_status_t dh_resources_init(void) {
  zx_status_t status;

  // Try dedicated resource protocols first.
  status = AcquireResource("/svc/fuchsia.kernel.IoportResource",
                           kIoportResourceGetOrdinal, &g_ioport_resource);
  if (status == ZX_OK) {
    fprintf(stderr, "driverhub: acquired I/O port resource\n");
  } else {
    fprintf(stderr, "driverhub: I/O port resource not routed (%d)\n", status);
  }

  status = AcquireResource("/svc/fuchsia.kernel.MmioResource",
                           kMmioResourceGetOrdinal, &g_mmio_resource);
  if (status == ZX_OK) {
    fprintf(stderr, "driverhub: acquired MMIO resource\n");
  } else {
    fprintf(stderr, "driverhub: MMIO resource not routed (%d)\n", status);
  }

  status = AcquireResource("/svc/fuchsia.kernel.VmexResource",
                           kVmexResourceGetOrdinal, &g_vmex_resource);
  if (status == ZX_OK) {
    fprintf(stderr, "driverhub: acquired VMEX resource\n");
  } else {
    fprintf(stderr, "driverhub: VMEX resource not routed (%d)\n", status);
  }

  // If dedicated resources aren't available (common for console programs),
  // try the DebugResource which is often routed to dev/debug namespaces.
  // On development builds, it may serve as a superset resource.
  if (g_ioport_resource == ZX_HANDLE_INVALID ||
      g_mmio_resource == ZX_HANDLE_INVALID) {
    zx_handle_t debug_resource = ZX_HANDLE_INVALID;
    status = AcquireResource("/svc/fuchsia.kernel.DebugResource",
                             kDebugResourceGetOrdinal, &debug_resource);
    if (status == ZX_OK) {
      fprintf(stderr, "driverhub: acquired debug resource, "
              "using as fallback for I/O\n");
      // Try using the debug resource for ioport and MMIO access.
      // On some builds, zx_resource_create can derive child resources.
      // As a simpler approach, try using it directly — if the kernel
      // accepts it for zx_ioports_request, great.
      if (g_ioport_resource == ZX_HANDLE_INVALID) {
        // Duplicate the handle so we can use it for ioport access too.
        zx_handle_t dup;
        if (zx_handle_duplicate(debug_resource, ZX_RIGHT_SAME_RIGHTS, &dup) ==
            ZX_OK) {
          g_ioport_resource = dup;
        }
      }
      if (g_mmio_resource == ZX_HANDLE_INVALID) {
        zx_handle_t dup;
        if (zx_handle_duplicate(debug_resource, ZX_RIGHT_SAME_RIGHTS, &dup) ==
            ZX_OK) {
          g_mmio_resource = dup;
        }
      }
      zx_handle_close(debug_resource);
    } else {
      fprintf(stderr, "driverhub: debug resource unavailable (%d)\n", status);
    }
  }

  // Log final resource status.
  fprintf(stderr, "driverhub: resources: ioport=%s mmio=%s vmex=%s\n",
          g_ioport_resource != ZX_HANDLE_INVALID ? "yes" : "no",
          g_mmio_resource != ZX_HANDLE_INVALID ? "yes" : "no",
          g_vmex_resource != ZX_HANDLE_INVALID ? "yes" : "no");

  return ZX_OK;  // Individual failures are non-fatal.
}

zx_handle_t dh_ioport_resource(void) { return g_ioport_resource; }
zx_handle_t dh_mmio_resource(void) { return g_mmio_resource; }
zx_handle_t dh_vmex_resource(void) { return g_vmex_resource; }

zx_status_t dh_ioports_request(uint16_t io_addr, uint32_t len) {
  if (g_ioport_resource == ZX_HANDLE_INVALID) {
    return ZX_ERR_NOT_FOUND;
  }
  return zx_ioports_request(g_ioport_resource, io_addr, len);
}

}  // extern "C"
