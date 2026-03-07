// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_NVMEM_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_NVMEM_H_

// Non-Volatile Memory (NVMEM) subsystem shim: EEPROM, OTP, fuses.
//
// Provides:
//   nvmem_register, devm_nvmem_register, nvmem_unregister
//   nvmem_cell_get, nvmem_cell_put, nvmem_cell_read, nvmem_cell_write
//   devm_nvmem_cell_get
//   nvmem_device_read, nvmem_device_write
//
// On Fuchsia: could map to fuchsia.hardware.nvram or stubs.

#include <cstddef>
#include <cstdint>

struct device;

#ifdef __cplusplus
extern "C" {
#endif

struct nvmem_device;
struct nvmem_cell;

typedef int (*nvmem_reg_read_t)(void*, unsigned int, void*, size_t);
typedef int (*nvmem_reg_write_t)(void*, unsigned int, void*, size_t);

struct nvmem_cell_info {
  const char* name;
  unsigned int offset;
  unsigned int bytes;
  unsigned int bit_offset;
  unsigned int nbits;
};

struct nvmem_config {
  struct device* dev;
  const char* name;
  int id;
  void* owner;
  const struct nvmem_cell_info* cells;
  int ncells;
  int type;
  int read_only;
  int root_only;
  int no_of_node;
  nvmem_reg_read_t reg_read;
  nvmem_reg_write_t reg_write;
  int size;
  int word_size;
  int stride;
  void* priv;
};

// NVMEM types.
#define NVMEM_TYPE_UNKNOWN  0
#define NVMEM_TYPE_EEPROM   1
#define NVMEM_TYPE_OTP      2
#define NVMEM_TYPE_BATTERY_BACKED 3

struct nvmem_device* nvmem_register(const struct nvmem_config* config);
struct nvmem_device* devm_nvmem_register(struct device* dev,
                                          const struct nvmem_config* config);
void nvmem_unregister(struct nvmem_device* nvmem);

// Cell-based access.
struct nvmem_cell* nvmem_cell_get(struct device* dev, const char* id);
struct nvmem_cell* devm_nvmem_cell_get(struct device* dev, const char* id);
void nvmem_cell_put(struct nvmem_cell* cell);
void* nvmem_cell_read(struct nvmem_cell* cell, size_t* len);
int nvmem_cell_write(struct nvmem_cell* cell, void* buf, size_t len);

// Direct device access.
int nvmem_device_read(struct nvmem_device* nvmem, unsigned int offset,
                      size_t bytes, void* buf);
int nvmem_device_write(struct nvmem_device* nvmem, unsigned int offset,
                       size_t bytes, void* buf);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_NVMEM_H_
