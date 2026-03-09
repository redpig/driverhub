// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_DEVICE_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_DEVICE_H_

// KMI shims for the Linux device model.
//
// Provides: struct device, struct resource, devm_kmalloc, devm_kzalloc,
//           dev_set_drvdata, dev_get_drvdata, IORESOURCE_MEM, IORESOURCE_IRQ
//
// struct device is intentionally minimal — just enough to support the APIs
// used by GKI modules (driver data, dev_name, parent).

#include <stddef.h>
#include <stdint.h>

#include "src/shim/include/linux/android_kabi.h"

#ifdef __cplusplus
extern "C" {
#endif

// Minimal struct device matching the Linux kernel fields used by GKI modules.
//
// GKI 6.6 struct device on ARM64 is ~728 bytes with 8 ANDROID_KABI_RESERVE
// slots. We keep a simplified layout with key fields that modules access
// (init_name, parent, driver_data, platform_data) and pad to the correct
// total size.
//
// TODO: Validate field offsets against GKI 6.6 vmlinux. The offsets here
// match the simplified shim; the abi/structs.h version has the full
// offset-accurate layout for modules that access device fields directly.
struct device {
  const char* init_name;
  struct device* parent;
  void* driver_data;
  void* platform_data;
  // Devres (device-managed resource) list head. Simplified to a linked list.
  void* devres_head;
  // Padding to approximate GKI 6.6 struct device size.
  // Full ABI-compatible layout is in src/shim/include/linux/abi/structs.h.
  char _kabi_pad[728 - 5 * sizeof(void*) - 8 * sizeof(uint64_t)];
  // GKI ANDROID_KABI_RESERVE slots.
  ANDROID_KABI_RESERVE(1);
  ANDROID_KABI_RESERVE(2);
  ANDROID_KABI_RESERVE(3);
  ANDROID_KABI_RESERVE(4);
  ANDROID_KABI_RESERVE(5);
  ANDROID_KABI_RESERVE(6);
  ANDROID_KABI_RESERVE(7);
  ANDROID_KABI_RESERVE(8);
};

// Resource types.
#define IORESOURCE_MEM  0x00000200
#define IORESOURCE_IRQ  0x00000400

struct resource {
  uint64_t start;
  uint64_t end;
  const char* name;
  unsigned long flags;
};

static inline void* dev_get_drvdata(const struct device* dev) {
  return dev->driver_data;
}

static inline void dev_set_drvdata(struct device* dev, void* data) {
  dev->driver_data = data;
}

static inline const char* dev_name(const struct device* dev) {
  return dev->init_name ? dev->init_name : "(unknown)";
}

// Device-managed memory allocation. Memory is freed automatically when the
// device is removed. Simplified: we just use regular allocation and rely on
// module teardown to clean up.
void* devm_kmalloc(struct device* dev, size_t size, unsigned int flags);
void* devm_kzalloc(struct device* dev, size_t size, unsigned int flags);

// Device lifecycle.
void device_initialize(struct device* dev);
int device_add(struct device* dev);
void device_del(struct device* dev);
void put_device(struct device* dev);
int dev_set_name(struct device* dev, const char* fmt, ...);

// Class registration (rfkill uses direct class_register/unregister).
struct dh_class;
int class_register(struct dh_class* cls);
void class_unregister(struct dh_class* cls);

// Error pointer helpers (Linux convention: errors encoded as pointers).
#define MAX_ERRNO 4095

static inline void* ERR_PTR(long error) {
  return (void*)error;
}

static inline long PTR_ERR(const void* ptr) {
  return (long)ptr;
}

static inline int IS_ERR(const void* ptr) {
  return (unsigned long)ptr >= (unsigned long)-MAX_ERRNO;
}

static inline int IS_ERR_OR_NULL(const void* ptr) {
  return !ptr || IS_ERR(ptr);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_DEVICE_H_
