// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_FIRMWARE_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_FIRMWARE_H_

// KMI shims for the Linux firmware loading API.
//
// Provides: request_firmware, request_firmware_nowait, release_firmware,
//           firmware_request_nowarn, request_firmware_direct
//
// Firmware loading strategy:
//   1. Try local package path: /pkg/data/firmware/<name>
//   2. Fall back to the FirmwareLoader FIDL protocol served by Starnix or
//      a standalone firmware service component.
//
// See fidl/fuchsia.driverhub/firmware_loader.fidl for the FIDL definition.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct device;
struct module;

typedef unsigned int gfp_t;

// Linux firmware descriptor. Returned by request_firmware() and friends.
// The caller must pass this to release_firmware() when done.
struct firmware {
  size_t size;
  const uint8_t *data;
  void *priv;  // Internal: VMO handle storage
};

// Request a firmware blob synchronously. Blocks until the firmware is loaded
// or an error occurs.
// Returns 0 on success, negative errno on failure.
int request_firmware(const struct firmware **fw, const char *name,
                     struct device *dev);

// Request firmware asynchronously. The callback |cont| is invoked on a
// background thread when loading completes (or fails, in which case |fw|
// is NULL).
// Returns 0 if the async request was successfully dispatched.
int request_firmware_nowait(struct module *module, bool uevent,
                            const char *name, struct device *dev,
                            gfp_t gfp, void *context,
                            void (*cont)(const struct firmware *fw,
                                         void *context));

// Release a firmware blob previously obtained via request_firmware().
// Frees the data buffer and closes any underlying VMO handles.
void release_firmware(const struct firmware *fw);

// Request firmware without logging a warning on failure.
// Identical to request_firmware() except it suppresses the warning log.
int firmware_request_nowarn(const struct firmware **fw, const char *name,
                            struct device *dev);

// Request firmware with direct loading only (no fallback to usermode helper).
// In DriverHub, this behaves identically to request_firmware().
int request_firmware_direct(const struct firmware **fw, const char *name,
                            struct device *dev);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_FIRMWARE_H_
