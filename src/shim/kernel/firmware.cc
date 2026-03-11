// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/firmware.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <thread>

#include <fidl/fuchsia.driverhub/cpp/fidl.h>
#include <lib/component/incoming/cpp/protocol.h>
#include <lib/zx/vmo.h>

#include "src/shim/kernel/print.h"

// Maximum firmware path length: "/pkg/data/firmware/" prefix + name.
static constexpr size_t kFirmwarePathMax = 1024 + 32;

// Prefix for locally-packaged firmware blobs.
static constexpr const char* kLocalFirmwarePrefix = "/pkg/data/firmware/";

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Attempt to load firmware from the local package resource directory.
// Returns 0 on success and populates |*fw_out|, or -ENOENT if not found.
static int load_firmware_local(const char* name,
                               const struct firmware** fw_out) {
  char path[kFirmwarePathMax];
  int ret = snprintf(path, sizeof(path), "%s%s", kLocalFirmwarePrefix, name);
  if (ret < 0 || static_cast<size_t>(ret) >= sizeof(path)) {
    return -ENAMETOOLONG;
  }

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -ENOENT;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return -EIO;
  }

  size_t size = static_cast<size_t>(st.st_size);
  uint8_t* buf = static_cast<uint8_t*>(malloc(size));
  if (!buf) {
    close(fd);
    return -ENOMEM;
  }

  size_t total_read = 0;
  while (total_read < size) {
    ssize_t n = read(fd, buf + total_read, size - total_read);
    if (n <= 0) {
      free(buf);
      close(fd);
      return -EIO;
    }
    total_read += static_cast<size_t>(n);
  }
  close(fd);

  auto* fw = static_cast<struct firmware*>(calloc(1, sizeof(struct firmware)));
  if (!fw) {
    free(buf);
    return -ENOMEM;
  }

  fw->size = size;
  fw->data = buf;
  fw->priv = nullptr;  // No VMO for local loads.

  *fw_out = fw;
  return 0;
}

// Attempt to load firmware via the FirmwareLoader FIDL protocol.
// Returns 0 on success and populates |*fw_out|, or negative errno on failure.
static int load_firmware_fidl(const char* name,
                              const struct firmware** fw_out) {
  auto client_end = component::Connect<fuchsia_driverhub::FirmwareLoader>();
  if (client_end.is_error()) {
    printk(KERN_ERR "firmware: failed to connect to FirmwareLoader: %d\n",
           client_end.error_value());
    return -ENODEV;
  }

  fidl::SyncClient client(std::move(*client_end));

  auto result = client->RequestFirmware({{.name = std::string(name)}});
  if (result.is_error()) {
    // Don't log here — callers like firmware_request_nowarn rely on silence.
    return -ENOENT;
  }

  zx::vmo& vmo = result->firmware();
  uint64_t fw_size = result->size();

  // Map the VMO contents into a malloc'd buffer. This keeps the shim simple
  // and avoids holding the VMO mapped for the lifetime of the firmware, which
  // could complicate teardown.
  uint8_t* buf = static_cast<uint8_t*>(malloc(fw_size));
  if (!buf) {
    return -ENOMEM;
  }

  zx_status_t status = vmo.read(buf, 0, fw_size);
  if (status != ZX_OK) {
    printk(KERN_ERR "firmware: VMO read failed: %d\n", status);
    free(buf);
    return -EIO;
  }

  auto* fw = static_cast<struct firmware*>(calloc(1, sizeof(struct firmware)));
  if (!fw) {
    free(buf);
    return -ENOMEM;
  }

  fw->size = static_cast<size_t>(fw_size);
  fw->data = buf;
  fw->priv = nullptr;

  *fw_out = fw;
  return 0;
}

// Core firmware loading: try local first, then FIDL.
static int load_firmware_core(const char* name,
                              const struct firmware** fw_out,
                              bool warn_on_error) {
  if (!fw_out || !name) {
    return -EINVAL;
  }

  *fw_out = nullptr;

  // Strategy 1: Local package resource.
  int ret = load_firmware_local(name, fw_out);
  if (ret == 0) {
    printk(KERN_INFO "firmware: loaded '%s' from local package (%zu bytes)\n",
           name, (*fw_out)->size);
    return 0;
  }

  // Strategy 2: FIDL FirmwareLoader service.
  ret = load_firmware_fidl(name, fw_out);
  if (ret == 0) {
    printk(KERN_INFO "firmware: loaded '%s' via FIDL (%zu bytes)\n",
           name, (*fw_out)->size);
    return 0;
  }

  if (warn_on_error) {
    printk(KERN_ERR "firmware: failed to load '%s' (err %d)\n", name, ret);
  }

  return ret;
}

// ---------------------------------------------------------------------------
// Exported KMI shim functions
// ---------------------------------------------------------------------------

extern "C" {

int request_firmware(const struct firmware** fw, const char* name,
                     struct device* dev) {
  (void)dev;  // Device context unused in userspace shim.
  return load_firmware_core(name, fw, /*warn_on_error=*/true);
}

int request_firmware_nowait(struct module* module, bool uevent,
                            const char* name, struct device* dev,
                            gfp_t gfp, void* context,
                            void (*cont)(const struct firmware* fw,
                                         void* context)) {
  (void)module;
  (void)uevent;
  (void)dev;
  (void)gfp;

  if (!cont || !name) {
    return -EINVAL;
  }

  // Capture parameters by value for the background thread.
  std::string name_copy(name);
  std::thread([name_copy, context, cont]() {
    const struct firmware* fw = nullptr;
    int ret = load_firmware_core(name_copy.c_str(), &fw,
                                 /*warn_on_error=*/true);
    if (ret != 0) {
      fw = nullptr;  // Signal failure to callback.
    }
    cont(fw, context);
  }).detach();

  return 0;
}

void release_firmware(const struct firmware* fw) {
  if (!fw) {
    return;
  }
  // Free the data buffer (allocated by load_firmware_local or
  // load_firmware_fidl).
  free(const_cast<uint8_t*>(fw->data));
  // Free the firmware descriptor itself.
  free(const_cast<struct firmware*>(fw));
}

int firmware_request_nowarn(const struct firmware** fw, const char* name,
                            struct device* dev) {
  (void)dev;
  return load_firmware_core(name, fw, /*warn_on_error=*/false);
}

int request_firmware_direct(const struct firmware** fw, const char* name,
                            struct device* dev) {
  (void)dev;
  // In DriverHub there is no usermode helper fallback, so "direct" loading
  // is identical to the standard path.
  return load_firmware_core(name, fw, /*warn_on_error=*/true);
}

}  // extern "C"
