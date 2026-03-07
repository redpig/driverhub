// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Thermal framework shim: thermal zones, cooling devices, and hwmon.
//
// In userspace, temperature is simulated (constant 25°C).
// On Fuchsia: maps to fuchsia.hardware.thermal/Device FIDL.

#include "src/shim/subsystem/thermal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct thermal_zone_impl {
  char type[64];
  void* devdata;
  struct thermal_zone_device_ops* ops;
  int ntrips;
  int id;
};

struct thermal_cooling_impl {
  char type[64];
  void* devdata;
  const struct thermal_cooling_device_ops* ops;
  int id;
};

int g_tz_next_id = 0;
int g_cdev_next_id = 0;

}  // namespace

extern "C" {

struct thermal_zone_device* thermal_zone_device_register(
    const char* type, int ntrips, int /*mask*/,
    void* devdata, struct thermal_zone_device_ops* ops,
    struct thermal_zone_params* /*tzp*/,
    int /*passive_delay*/, int /*polling_delay*/) {
  auto* tz = static_cast<thermal_zone_impl*>(
      calloc(1, sizeof(thermal_zone_impl)));
  if (!tz)
    return nullptr;
  snprintf(tz->type, sizeof(tz->type), "%s", type ? type : "unknown");
  tz->devdata = devdata;
  tz->ops = ops;
  tz->ntrips = ntrips;
  tz->id = g_tz_next_id++;
  fprintf(stderr, "driverhub: thermal: registered zone '%s' (id=%d, trips=%d)\n",
          tz->type, tz->id, ntrips);
  return reinterpret_cast<struct thermal_zone_device*>(tz);
}

struct thermal_zone_device* thermal_zone_device_register_with_trips(
    const char* type, const struct thermal_trip* /*trips*/, int ntrips,
    void* devdata, struct thermal_zone_device_ops* ops,
    struct thermal_zone_params* tzp,
    int passive_delay, int polling_delay) {
  return thermal_zone_device_register(type, ntrips, 0, devdata, ops,
                                       tzp, passive_delay, polling_delay);
}

struct thermal_zone_device* devm_thermal_of_zone_register(
    struct device* /*dev*/, int sensor_id, void* data,
    struct thermal_zone_device_ops* ops) {
  char name[64];
  snprintf(name, sizeof(name), "of-sensor-%d", sensor_id);
  return thermal_zone_device_register(name, 0, 0, data, ops,
                                       nullptr, 0, 0);
}

void thermal_zone_device_unregister(struct thermal_zone_device* tz) {
  if (!tz) return;
  auto* impl = reinterpret_cast<thermal_zone_impl*>(tz);
  fprintf(stderr, "driverhub: thermal: unregistered zone '%s'\n", impl->type);
  free(impl);
}

void thermal_zone_device_update(struct thermal_zone_device* /*tz*/,
                                int /*event*/) {
  // No-op in userspace; Fuchsia would poll the sensor.
}

void* thermal_zone_device_priv(struct thermal_zone_device* tz) {
  if (!tz) return nullptr;
  return reinterpret_cast<thermal_zone_impl*>(tz)->devdata;
}

// --- Cooling device ---

struct thermal_cooling_device* thermal_cooling_device_register(
    const char* type, void* devdata,
    const struct thermal_cooling_device_ops* ops) {
  auto* cdev = static_cast<thermal_cooling_impl*>(
      calloc(1, sizeof(thermal_cooling_impl)));
  if (!cdev)
    return nullptr;
  snprintf(cdev->type, sizeof(cdev->type), "%s", type ? type : "unknown");
  cdev->devdata = devdata;
  cdev->ops = ops;
  cdev->id = g_cdev_next_id++;
  fprintf(stderr, "driverhub: thermal: registered cooling device '%s' (id=%d)\n",
          cdev->type, cdev->id);
  return reinterpret_cast<struct thermal_cooling_device*>(cdev);
}

struct thermal_cooling_device* devm_thermal_of_cooling_device_register(
    struct device* /*dev*/, void* /*np*/, const char* type, void* devdata,
    const struct thermal_cooling_device_ops* ops) {
  return thermal_cooling_device_register(type, devdata, ops);
}

void thermal_cooling_device_unregister(struct thermal_cooling_device* cdev) {
  if (!cdev) return;
  auto* impl = reinterpret_cast<thermal_cooling_impl*>(cdev);
  fprintf(stderr, "driverhub: thermal: unregistered cooling device '%s'\n",
          impl->type);
  free(impl);
}

// --- hwmon ---

struct hwmon_impl {
  char name[64];
  void* drvdata;
};

struct device* hwmon_device_register_with_info(
    struct device* /*dev*/, const char* name, void* drvdata,
    const struct hwmon_chip_info* /*chip*/, const void** /*groups*/) {
  auto* hwmon = static_cast<hwmon_impl*>(calloc(1, sizeof(hwmon_impl)));
  if (!hwmon)
    return nullptr;
  snprintf(hwmon->name, sizeof(hwmon->name), "%s", name ? name : "unknown");
  hwmon->drvdata = drvdata;
  fprintf(stderr, "driverhub: hwmon: registered '%s'\n", hwmon->name);
  return reinterpret_cast<struct device*>(hwmon);
}

void hwmon_device_unregister(struct device* hwmon) {
  if (!hwmon) return;
  auto* impl = reinterpret_cast<hwmon_impl*>(hwmon);
  fprintf(stderr, "driverhub: hwmon: unregistered '%s'\n", impl->name);
  free(impl);
}

struct device* devm_hwmon_device_register_with_info(
    struct device* dev, const char* name, void* drvdata,
    const struct hwmon_chip_info* chip, const void** groups) {
  return hwmon_device_register_with_info(dev, name, drvdata, chip, groups);
}

}  // extern "C"
