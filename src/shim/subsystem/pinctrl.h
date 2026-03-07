// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_PINCTRL_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_PINCTRL_H_

// Pin control subsystem shim: pin muxing and configuration.
//
// Provides:
//   pinctrl_get, pinctrl_put, devm_pinctrl_get
//   pinctrl_lookup_state, pinctrl_select_state
//   pinctrl_register, pinctrl_unregister, devm_pinctrl_register
//   pinctrl_gpio_request, pinctrl_gpio_free
//
// On Fuchsia: maps to fuchsia.hardware.pin or stubs.

struct device;

#ifdef __cplusplus
extern "C" {
#endif

struct pinctrl;
struct pinctrl_state;
struct pinctrl_dev;

struct pinctrl_pin_desc {
  unsigned int number;
  const char* name;
  void* drv_data;
};

struct pinctrl_ops {
  int (*get_groups_count)(struct pinctrl_dev*);
  const char* (*get_group_name)(struct pinctrl_dev*, unsigned);
  int (*get_group_pins)(struct pinctrl_dev*, unsigned,
                        const unsigned**, unsigned*);
  int (*dt_node_to_map)(struct pinctrl_dev*, void* /*np*/,
                        void* /*map*/, unsigned*);
  void (*dt_free_map)(struct pinctrl_dev*, void* /*map*/, unsigned);
};

struct pinmux_ops {
  int (*request)(struct pinctrl_dev*, unsigned);
  int (*free)(struct pinctrl_dev*, unsigned);
  int (*get_functions_count)(struct pinctrl_dev*);
  const char* (*get_function_name)(struct pinctrl_dev*, unsigned);
  int (*get_function_groups)(struct pinctrl_dev*, unsigned,
                             const char* const**, unsigned*);
  int (*set_mux)(struct pinctrl_dev*, unsigned, unsigned);
  int (*gpio_request_enable)(struct pinctrl_dev*, void*, unsigned);
  void (*gpio_disable_free)(struct pinctrl_dev*, void*, unsigned);
};

struct pinconf_ops {
  int (*pin_config_get)(struct pinctrl_dev*, unsigned, unsigned long*);
  int (*pin_config_set)(struct pinctrl_dev*, unsigned,
                        unsigned long*, unsigned);
  int (*pin_config_group_get)(struct pinctrl_dev*, unsigned, unsigned long*);
  int (*pin_config_group_set)(struct pinctrl_dev*, unsigned,
                              unsigned long*, unsigned);
};

struct pinctrl_desc {
  const char* name;
  const struct pinctrl_pin_desc* pins;
  unsigned int npins;
  const struct pinctrl_ops* pctlops;
  const struct pinmux_ops* pmxops;
  const struct pinconf_ops* confops;
  void* owner;
};

// Consumer API.
struct pinctrl* pinctrl_get(struct device* dev);
struct pinctrl* devm_pinctrl_get(struct device* dev);
void pinctrl_put(struct pinctrl* p);
struct pinctrl_state* pinctrl_lookup_state(struct pinctrl* p,
                                            const char* name);
int pinctrl_select_state(struct pinctrl* p, struct pinctrl_state* state);
int pinctrl_pm_select_default_state(struct device* dev);
int pinctrl_pm_select_sleep_state(struct device* dev);

// Provider API.
struct pinctrl_dev* pinctrl_register(struct pinctrl_desc* desc,
                                      struct device* dev, void* driver_data);
struct pinctrl_dev* devm_pinctrl_register(struct device* dev,
                                           struct pinctrl_desc* desc,
                                           void* driver_data);
void pinctrl_unregister(struct pinctrl_dev* pctldev);

// GPIO integration.
int pinctrl_gpio_request(unsigned gpio);
void pinctrl_gpio_free(unsigned gpio);
int pinctrl_gpio_direction_input(unsigned gpio);
int pinctrl_gpio_direction_output(unsigned gpio);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_PINCTRL_H_
