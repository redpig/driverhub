// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pin control subsystem shim.
//
// In userspace, pin states are tracked but no hardware is configured.
// On Fuchsia: maps to fuchsia.hardware.pin FIDL.

#include "src/shim/subsystem/pinctrl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct pinctrl_impl {
  char name[64];
};

struct pinctrl_state_impl {
  char name[64];
};

struct pinctrl_dev_impl {
  struct pinctrl_desc* desc;
  void* driver_data;
  int id;
};

int g_pctldev_next_id = 0;

}  // namespace

extern "C" {

// --- Consumer API ---

struct pinctrl* pinctrl_get(struct device* /*dev*/) {
  auto* p = static_cast<pinctrl_impl*>(calloc(1, sizeof(pinctrl_impl)));
  if (!p) return nullptr;
  snprintf(p->name, sizeof(p->name), "pinctrl");
  return reinterpret_cast<struct pinctrl*>(p);
}

struct pinctrl* devm_pinctrl_get(struct device* dev) {
  return pinctrl_get(dev);
}

void pinctrl_put(struct pinctrl* p) {
  free(p);
}

struct pinctrl_state* pinctrl_lookup_state(struct pinctrl* /*p*/,
                                            const char* name) {
  auto* state = static_cast<pinctrl_state_impl*>(
      calloc(1, sizeof(pinctrl_state_impl)));
  if (!state) return nullptr;
  snprintf(state->name, sizeof(state->name), "%s", name ? name : "default");
  return reinterpret_cast<struct pinctrl_state*>(state);
}

int pinctrl_select_state(struct pinctrl* /*p*/, struct pinctrl_state* state) {
  if (!state) return -22;  // -EINVAL
  auto* impl = reinterpret_cast<pinctrl_state_impl*>(state);
  fprintf(stderr, "driverhub: pinctrl: select state '%s'\n", impl->name);
  return 0;
}

int pinctrl_pm_select_default_state(struct device* /*dev*/) {
  return 0;
}

int pinctrl_pm_select_sleep_state(struct device* /*dev*/) {
  return 0;
}

// --- Provider API ---

struct pinctrl_dev* pinctrl_register(struct pinctrl_desc* desc,
                                      struct device* /*dev*/,
                                      void* driver_data) {
  auto* pctldev = static_cast<pinctrl_dev_impl*>(
      calloc(1, sizeof(pinctrl_dev_impl)));
  if (!pctldev) return nullptr;
  pctldev->desc = desc;
  pctldev->driver_data = driver_data;
  pctldev->id = g_pctldev_next_id++;
  fprintf(stderr, "driverhub: pinctrl: registered '%s' (%u pins)\n",
          desc->name ? desc->name : "unknown", desc->npins);
  return reinterpret_cast<struct pinctrl_dev*>(pctldev);
}

struct pinctrl_dev* devm_pinctrl_register(struct device* dev,
                                           struct pinctrl_desc* desc,
                                           void* driver_data) {
  return pinctrl_register(desc, dev, driver_data);
}

void pinctrl_unregister(struct pinctrl_dev* pctldev) {
  if (!pctldev) return;
  auto* impl = reinterpret_cast<pinctrl_dev_impl*>(pctldev);
  fprintf(stderr, "driverhub: pinctrl: unregistered (id=%d)\n", impl->id);
  free(impl);
}

// --- GPIO integration ---

int pinctrl_gpio_request(unsigned gpio) {
  (void)gpio;
  return 0;
}

void pinctrl_gpio_free(unsigned /*gpio*/) {}

int pinctrl_gpio_direction_input(unsigned /*gpio*/) {
  return 0;
}

int pinctrl_gpio_direction_output(unsigned /*gpio*/) {
  return 0;
}

}  // extern "C"
