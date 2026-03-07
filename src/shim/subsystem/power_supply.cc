// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Power supply subsystem shim: battery and charger.
//
// In userspace, power supply devices are tracked with simulated state.
// On Fuchsia: maps to fuchsia.power.battery FIDL.

#include "src/shim/subsystem/power_supply.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct power_supply_impl {
  char name[64];
  const struct power_supply_desc* desc;
  void* drv_data;
  int id;
};

int g_psy_next_id = 0;

}  // namespace

extern "C" {

struct power_supply* power_supply_register(
    struct device* /*parent*/, const struct power_supply_desc* desc,
    const struct power_supply_config* cfg) {
  if (!desc) return nullptr;
  auto* psy = static_cast<power_supply_impl*>(
      calloc(1, sizeof(power_supply_impl)));
  if (!psy) return nullptr;
  snprintf(psy->name, sizeof(psy->name), "%s",
           desc->name ? desc->name : "unknown");
  psy->desc = desc;
  psy->drv_data = cfg ? cfg->drv_data : nullptr;
  psy->id = g_psy_next_id++;
  fprintf(stderr, "driverhub: power_supply: registered '%s' (type=%d)\n",
          psy->name, desc->type);
  return reinterpret_cast<struct power_supply*>(psy);
}

struct power_supply* devm_power_supply_register(
    struct device* parent, const struct power_supply_desc* desc,
    const struct power_supply_config* cfg) {
  return power_supply_register(parent, desc, cfg);
}

void power_supply_unregister(struct power_supply* psy) {
  if (!psy) return;
  auto* impl = reinterpret_cast<power_supply_impl*>(psy);
  fprintf(stderr, "driverhub: power_supply: unregistered '%s'\n", impl->name);
  free(impl);
}

void power_supply_changed(struct power_supply* psy) {
  if (!psy) return;
  auto* impl = reinterpret_cast<power_supply_impl*>(psy);
  fprintf(stderr, "driverhub: power_supply: '%s' changed\n", impl->name);
}

void* power_supply_get_drvdata(struct power_supply* psy) {
  if (!psy) return nullptr;
  return reinterpret_cast<power_supply_impl*>(psy)->drv_data;
}

void power_supply_set_drvdata(struct power_supply* psy, void* data) {
  if (psy)
    reinterpret_cast<power_supply_impl*>(psy)->drv_data = data;
}

struct power_supply* power_supply_get_by_name(const char* name) {
  fprintf(stderr, "driverhub: power_supply: get_by_name '%s' (not found)\n",
          name ? name : "(null)");
  return nullptr;
}

void power_supply_put(struct power_supply* /*psy*/) {
  // Reference counting no-op.
}

int power_supply_get_property(struct power_supply* psy,
                              enum power_supply_property psp,
                              union power_supply_propval* val) {
  if (!psy || !val) return -22;
  auto* impl = reinterpret_cast<power_supply_impl*>(psy);
  if (impl->desc && impl->desc->get_property)
    return impl->desc->get_property(psy, psp, val);
  return -95;  // -EOPNOTSUPP
}

}  // extern "C"
