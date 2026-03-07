// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IIO (Industrial I/O) subsystem shim: sensor devices.
//
// In userspace, IIO devices are tracked but no actual hardware is read.
// On Fuchsia: maps to fuchsia.input.report or fuchsia.sensors.

#include "src/shim/subsystem/iio.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct iio_dev_impl {
  struct iio_dev pub;
  int id;
  void* priv_data;  // Flexible array member simulation.
};

struct iio_trigger_impl {
  char name[128];
  int id;
};

int g_iio_next_id = 0;
int g_trig_next_id = 0;

}  // namespace

extern "C" {

struct iio_dev* iio_device_alloc(struct device* parent, int sizeof_priv) {
  size_t total = sizeof(iio_dev_impl) + sizeof_priv;
  auto* impl = static_cast<iio_dev_impl*>(calloc(1, total));
  if (!impl)
    return nullptr;
  impl->id = g_iio_next_id++;
  impl->pub.dev_parent = parent;
  impl->priv_data = reinterpret_cast<char*>(impl) + sizeof(iio_dev_impl);
  return &impl->pub;
}

struct iio_dev* devm_iio_device_alloc(struct device* parent, int sizeof_priv) {
  return iio_device_alloc(parent, sizeof_priv);
}

void iio_device_free(struct iio_dev* indio_dev) {
  if (!indio_dev) return;
  auto* impl = reinterpret_cast<iio_dev_impl*>(indio_dev);
  free(impl);
}

int iio_device_register(struct iio_dev* indio_dev) {
  if (!indio_dev) return -22;  // -EINVAL
  auto* impl = reinterpret_cast<iio_dev_impl*>(indio_dev);
  fprintf(stderr, "driverhub: iio: registered device '%s' (id=%d, channels=%d)\n",
          indio_dev->name ? indio_dev->name : "unknown",
          impl->id, indio_dev->num_channels);
  return 0;
}

int devm_iio_device_register(struct device* /*dev*/,
                              struct iio_dev* indio_dev) {
  return iio_device_register(indio_dev);
}

void iio_device_unregister(struct iio_dev* indio_dev) {
  if (!indio_dev) return;
  auto* impl = reinterpret_cast<iio_dev_impl*>(indio_dev);
  fprintf(stderr, "driverhub: iio: unregistered device '%s'\n",
          indio_dev->name ? indio_dev->name : "unknown");
  (void)impl;
}

void* iio_priv(const struct iio_dev* indio_dev) {
  if (!indio_dev) return nullptr;
  auto* impl = reinterpret_cast<const iio_dev_impl*>(indio_dev);
  return const_cast<void*>(
      static_cast<const void*>(
          reinterpret_cast<const char*>(impl) + sizeof(iio_dev_impl)));
}

int iio_push_to_buffers_with_timestamp(struct iio_dev* /*indio_dev*/,
                                        void* /*data*/, int64_t /*timestamp*/) {
  return 0;  // Data accepted (but not forwarded in userspace shim).
}

struct iio_trigger* devm_iio_trigger_alloc(struct device* /*dev*/,
                                            const char* fmt, ...) {
  auto* trig = static_cast<iio_trigger_impl*>(
      calloc(1, sizeof(iio_trigger_impl)));
  if (!trig) return nullptr;
  va_list args;
  va_start(args, fmt);
  vsnprintf(trig->name, sizeof(trig->name), fmt, args);
  va_end(args);
  trig->id = g_trig_next_id++;
  return reinterpret_cast<struct iio_trigger*>(trig);
}

int devm_iio_trigger_register(struct device* /*dev*/,
                               struct iio_trigger* trig) {
  if (!trig) return -22;
  auto* impl = reinterpret_cast<iio_trigger_impl*>(trig);
  fprintf(stderr, "driverhub: iio: registered trigger '%s'\n", impl->name);
  return 0;
}

void iio_trigger_notify_done(struct iio_trigger* /*trig*/) {
  // No-op.
}

}  // extern "C"
