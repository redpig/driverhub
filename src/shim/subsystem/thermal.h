// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_THERMAL_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_THERMAL_H_

// Thermal framework shim: thermal zones, cooling devices, and hwmon.
//
// Provides:
//   thermal_zone_device_register, thermal_zone_device_unregister
//   thermal_cooling_device_register, thermal_cooling_device_unregister
//   thermal_zone_device_update
//   devm_thermal_of_zone_register
//   hwmon_device_register_with_info
//
// On Fuchsia: maps to fuchsia.hardware.thermal/Device or stubs.

#include <cstdint>

struct device;

#ifdef __cplusplus
extern "C" {
#endif

// --- Thermal zone ---

struct thermal_zone_device;
struct thermal_cooling_device;

enum thermal_trip_type {
  THERMAL_TRIP_ACTIVE = 0,
  THERMAL_TRIP_PASSIVE,
  THERMAL_TRIP_HOT,
  THERMAL_TRIP_CRITICAL,
};

struct thermal_trip {
  int temperature;
  int hysteresis;
  enum thermal_trip_type type;
};

struct thermal_zone_device_ops {
  int (*get_temp)(struct thermal_zone_device*, int*);
  int (*set_trips)(struct thermal_zone_device*, int, int);
  int (*get_trend)(struct thermal_zone_device*, int,
                   enum thermal_trip_type*);
};

struct thermal_zone_params {
  const char* governor_name;
  int no_hwmon;
};

struct thermal_zone_device* thermal_zone_device_register(
    const char* type, int ntrips, int mask,
    void* devdata, struct thermal_zone_device_ops* ops,
    struct thermal_zone_params* tzp,
    int passive_delay, int polling_delay);

struct thermal_zone_device* thermal_zone_device_register_with_trips(
    const char* type, const struct thermal_trip* trips, int ntrips,
    void* devdata, struct thermal_zone_device_ops* ops,
    struct thermal_zone_params* tzp,
    int passive_delay, int polling_delay);

struct thermal_zone_device* devm_thermal_of_zone_register(
    struct device* dev, int sensor_id, void* data,
    struct thermal_zone_device_ops* ops);

void thermal_zone_device_unregister(struct thermal_zone_device* tz);
void thermal_zone_device_update(struct thermal_zone_device* tz,
                                int event);
void* thermal_zone_device_priv(struct thermal_zone_device* tz);

// --- Cooling device ---

struct thermal_cooling_device_ops {
  int (*get_max_state)(struct thermal_cooling_device*, unsigned long*);
  int (*get_cur_state)(struct thermal_cooling_device*, unsigned long*);
  int (*set_cur_state)(struct thermal_cooling_device*, unsigned long);
};

struct thermal_cooling_device* thermal_cooling_device_register(
    const char* type, void* devdata,
    const struct thermal_cooling_device_ops* ops);

struct thermal_cooling_device* devm_thermal_of_cooling_device_register(
    struct device* dev, void* np, const char* type, void* devdata,
    const struct thermal_cooling_device_ops* ops);

void thermal_cooling_device_unregister(struct thermal_cooling_device* cdev);

// --- hwmon ---

struct hwmon_chip_info;

struct device* hwmon_device_register_with_info(
    struct device* dev, const char* name, void* drvdata,
    const struct hwmon_chip_info* chip, const void** groups);

void hwmon_device_unregister(struct device* hwmon);

struct device* devm_hwmon_device_register_with_info(
    struct device* dev, const char* name, void* drvdata,
    const struct hwmon_chip_info* chip, const void** groups);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_THERMAL_H_
