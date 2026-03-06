// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/device.h"

#include <cstdlib>
#include <cstring>

extern "C" {

void* devm_kmalloc(struct device* dev, size_t size, unsigned int flags) {
  (void)dev;    // TODO: Track for automatic devm cleanup on device removal.
  (void)flags;
  return malloc(size);
}

void* devm_kzalloc(struct device* dev, size_t size, unsigned int flags) {
  (void)dev;
  (void)flags;
  return calloc(1, size);
}

}  // extern "C"
