// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NVMEM subsystem shim: EEPROM, OTP, fuse data.
//
// In userspace, NVMEM is backed by zero-filled memory.
// On Fuchsia: could map to fuchsia.hardware.nvram FIDL.

#include "src/shim/subsystem/nvmem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct nvmem_device_impl {
  char name[64];
  int id;
  int size;
  int read_only;
  void* priv;
  nvmem_reg_read_t reg_read;
  nvmem_reg_write_t reg_write;
  uint8_t* data;  // Simulated storage.
};

struct nvmem_cell_impl {
  char name[64];
  struct nvmem_device_impl* nvmem;
  unsigned int offset;
  unsigned int bytes;
};

int g_nvmem_next_id = 0;

}  // namespace

extern "C" {

struct nvmem_device* nvmem_register(const struct nvmem_config* config) {
  if (!config) return nullptr;
  auto* impl = static_cast<nvmem_device_impl*>(
      calloc(1, sizeof(nvmem_device_impl)));
  if (!impl) return nullptr;
  snprintf(impl->name, sizeof(impl->name), "%s",
           config->name ? config->name : "unknown");
  impl->id = config->id >= 0 ? config->id : g_nvmem_next_id++;
  impl->size = config->size;
  impl->read_only = config->read_only;
  impl->priv = config->priv;
  impl->reg_read = config->reg_read;
  impl->reg_write = config->reg_write;
  if (config->size > 0) {
    impl->data = static_cast<uint8_t*>(calloc(1, config->size));
  }
  fprintf(stderr, "driverhub: nvmem: registered '%s' (id=%d, size=%d, ro=%d)\n",
          impl->name, impl->id, impl->size, impl->read_only);
  return reinterpret_cast<struct nvmem_device*>(impl);
}

struct nvmem_device* devm_nvmem_register(struct device* /*dev*/,
                                          const struct nvmem_config* config) {
  return nvmem_register(config);
}

void nvmem_unregister(struct nvmem_device* nvmem) {
  if (!nvmem) return;
  auto* impl = reinterpret_cast<nvmem_device_impl*>(nvmem);
  fprintf(stderr, "driverhub: nvmem: unregistered '%s'\n", impl->name);
  free(impl->data);
  free(impl);
}

struct nvmem_cell* nvmem_cell_get(struct device* /*dev*/, const char* id) {
  // In a real implementation, this would look up the cell by name.
  // Return a placeholder cell.
  auto* cell = static_cast<nvmem_cell_impl*>(
      calloc(1, sizeof(nvmem_cell_impl)));
  if (!cell) return nullptr;
  snprintf(cell->name, sizeof(cell->name), "%s", id ? id : "unknown");
  cell->bytes = 4;  // Default 4-byte cell.
  return reinterpret_cast<struct nvmem_cell*>(cell);
}

struct nvmem_cell* devm_nvmem_cell_get(struct device* dev, const char* id) {
  return nvmem_cell_get(dev, id);
}

void nvmem_cell_put(struct nvmem_cell* cell) {
  free(cell);
}

void* nvmem_cell_read(struct nvmem_cell* cell, size_t* len) {
  if (!cell) return nullptr;
  auto* impl = reinterpret_cast<nvmem_cell_impl*>(cell);
  size_t sz = impl->bytes ? impl->bytes : 4;
  void* buf = calloc(1, sz);
  if (len) *len = sz;
  return buf;
}

int nvmem_cell_write(struct nvmem_cell* cell, void* /*buf*/, size_t /*len*/) {
  if (!cell) return -22;
  return 0;
}

int nvmem_device_read(struct nvmem_device* nvmem, unsigned int offset,
                      size_t bytes, void* buf) {
  if (!nvmem || !buf) return -22;
  auto* impl = reinterpret_cast<nvmem_device_impl*>(nvmem);
  if (impl->reg_read)
    return impl->reg_read(impl->priv, offset, buf, bytes);
  if (impl->data && offset + bytes <= (size_t)impl->size) {
    memcpy(buf, impl->data + offset, bytes);
    return bytes;
  }
  memset(buf, 0, bytes);
  return bytes;
}

int nvmem_device_write(struct nvmem_device* nvmem, unsigned int offset,
                       size_t bytes, void* buf) {
  if (!nvmem || !buf) return -22;
  auto* impl = reinterpret_cast<nvmem_device_impl*>(nvmem);
  if (impl->read_only) return -1;  // -EPERM
  if (impl->reg_write)
    return impl->reg_write(impl->priv, offset, buf, bytes);
  if (impl->data && offset + bytes <= (size_t)impl->size) {
    memcpy(impl->data + offset, buf, bytes);
    return bytes;
  }
  return bytes;
}

}  // extern "C"
