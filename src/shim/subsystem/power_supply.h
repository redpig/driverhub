// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_POWER_SUPPLY_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_POWER_SUPPLY_H_

// Power supply subsystem shim: battery, charger, USB power delivery.
//
// Provides:
//   power_supply_register, devm_power_supply_register
//   power_supply_unregister
//   power_supply_changed, power_supply_get_drvdata
//   power_supply_get_by_name, power_supply_put
//
// On Fuchsia: maps to fuchsia.power.battery or stubs.

#include <cstddef>
#include <cstdint>

struct device;

#ifdef __cplusplus
extern "C" {
#endif

enum power_supply_type {
  POWER_SUPPLY_TYPE_UNKNOWN = 0,
  POWER_SUPPLY_TYPE_BATTERY,
  POWER_SUPPLY_TYPE_UPS,
  POWER_SUPPLY_TYPE_MAINS,
  POWER_SUPPLY_TYPE_USB,
  POWER_SUPPLY_TYPE_USB_DCP,
  POWER_SUPPLY_TYPE_USB_CDP,
  POWER_SUPPLY_TYPE_USB_ACA,
  POWER_SUPPLY_TYPE_USB_TYPE_C,
  POWER_SUPPLY_TYPE_USB_PD,
  POWER_SUPPLY_TYPE_USB_PD_DRP,
  POWER_SUPPLY_TYPE_WIRELESS,
};

enum power_supply_property {
  POWER_SUPPLY_PROP_STATUS = 0,
  POWER_SUPPLY_PROP_CHARGE_TYPE,
  POWER_SUPPLY_PROP_HEALTH,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_AUTHENTIC,
  POWER_SUPPLY_PROP_TECHNOLOGY,
  POWER_SUPPLY_PROP_CYCLE_COUNT,
  POWER_SUPPLY_PROP_VOLTAGE_MAX,
  POWER_SUPPLY_PROP_VOLTAGE_MIN,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_VOLTAGE_AVG,
  POWER_SUPPLY_PROP_VOLTAGE_OCV,
  POWER_SUPPLY_PROP_CURRENT_MAX,
  POWER_SUPPLY_PROP_CURRENT_NOW,
  POWER_SUPPLY_PROP_CURRENT_AVG,
  POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
  POWER_SUPPLY_PROP_CHARGE_FULL,
  POWER_SUPPLY_PROP_CHARGE_NOW,
  POWER_SUPPLY_PROP_CHARGE_COUNTER,
  POWER_SUPPLY_PROP_CAPACITY,
  POWER_SUPPLY_PROP_CAPACITY_LEVEL,
  POWER_SUPPLY_PROP_TEMP,
  POWER_SUPPLY_PROP_TEMP_MAX,
  POWER_SUPPLY_PROP_TEMP_MIN,
  POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
  POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
  POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
  POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
  POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
  POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
  POWER_SUPPLY_PROP_ENERGY_FULL,
  POWER_SUPPLY_PROP_ENERGY_NOW,
  POWER_SUPPLY_PROP_POWER_NOW,
  POWER_SUPPLY_PROP_USB_TYPE,
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_MANUFACTURER,
  POWER_SUPPLY_PROP_SERIAL_NUMBER,
};

// Status values.
#define POWER_SUPPLY_STATUS_UNKNOWN    0
#define POWER_SUPPLY_STATUS_CHARGING   1
#define POWER_SUPPLY_STATUS_DISCHARGING 2
#define POWER_SUPPLY_STATUS_NOT_CHARGING 3
#define POWER_SUPPLY_STATUS_FULL       4

// Health values.
#define POWER_SUPPLY_HEALTH_UNKNOWN    0
#define POWER_SUPPLY_HEALTH_GOOD       1
#define POWER_SUPPLY_HEALTH_OVERHEAT   2
#define POWER_SUPPLY_HEALTH_DEAD       3
#define POWER_SUPPLY_HEALTH_OVERVOLTAGE 4
#define POWER_SUPPLY_HEALTH_COLD       5

union power_supply_propval {
  int intval;
  const char* strval;
};

struct power_supply;

struct power_supply_desc {
  const char* name;
  enum power_supply_type type;
  const enum power_supply_property* properties;
  size_t num_properties;
  int (*get_property)(struct power_supply*,
                      enum power_supply_property,
                      union power_supply_propval*);
  int (*set_property)(struct power_supply*,
                      enum power_supply_property,
                      const union power_supply_propval*);
  int (*property_is_writeable)(struct power_supply*,
                               enum power_supply_property);
  void (*external_power_changed)(struct power_supply*);
};

struct power_supply_config {
  void* drv_data;
  const void** attr_grp;
  char** supplied_to;
  size_t num_supplicants;
};

struct power_supply* power_supply_register(
    struct device* parent, const struct power_supply_desc* desc,
    const struct power_supply_config* cfg);

struct power_supply* devm_power_supply_register(
    struct device* parent, const struct power_supply_desc* desc,
    const struct power_supply_config* cfg);

void power_supply_unregister(struct power_supply* psy);
void power_supply_changed(struct power_supply* psy);

void* power_supply_get_drvdata(struct power_supply* psy);
void power_supply_set_drvdata(struct power_supply* psy, void* data);

struct power_supply* power_supply_get_by_name(const char* name);
void power_supply_put(struct power_supply* psy);

int power_supply_get_property(struct power_supply* psy,
                              enum power_supply_property psp,
                              union power_supply_propval* val);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_POWER_SUPPLY_H_
