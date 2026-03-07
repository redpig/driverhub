// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_PWM_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_PWM_H_

// PWM (Pulse Width Modulation) subsystem shim.
//
// Provides:
//   pwm_request, pwm_free, pwm_get, devm_pwm_get, pwm_put
//   pwm_config, pwm_set_polarity, pwm_enable, pwm_disable
//   pwm_apply_state, pwm_get_state
//   pwmchip_add, pwmchip_remove
//
// On Fuchsia: maps to fuchsia.hardware.pwm or stubs.

#include <cstdint>

struct device;

#ifdef __cplusplus
extern "C" {
#endif

enum pwm_polarity {
  PWM_POLARITY_NORMAL = 0,
  PWM_POLARITY_INVERSED,
};

struct pwm_state {
  uint64_t period;       // nanoseconds
  uint64_t duty_cycle;   // nanoseconds
  enum pwm_polarity polarity;
  int enabled;
};

struct pwm_device {
  const char* label;
  unsigned int hwpwm;
  struct pwm_state state;
  void* chip;
};

struct pwm_chip;

struct pwm_ops {
  int (*request)(struct pwm_chip*, struct pwm_device*);
  void (*free)(struct pwm_chip*, struct pwm_device*);
  int (*apply)(struct pwm_chip*, struct pwm_device*,
               const struct pwm_state*);
  int (*get_state)(struct pwm_chip*, struct pwm_device*,
                   struct pwm_state*);
};

struct pwm_chip {
  struct device* dev;
  const struct pwm_ops* ops;
  unsigned int npwm;
  struct pwm_device* pwms;
};

// Consumer API.
struct pwm_device* pwm_request(int pwm, const char* label);
struct pwm_device* pwm_get(struct device* dev, const char* con_id);
struct pwm_device* devm_pwm_get(struct device* dev, const char* con_id);
void pwm_free(struct pwm_device* pwm);
void pwm_put(struct pwm_device* pwm);

int pwm_config(struct pwm_device* pwm, int duty_ns, int period_ns);
int pwm_set_polarity(struct pwm_device* pwm, enum pwm_polarity polarity);
int pwm_enable(struct pwm_device* pwm);
void pwm_disable(struct pwm_device* pwm);

int pwm_apply_state(struct pwm_device* pwm, const struct pwm_state* state);
void pwm_get_state(const struct pwm_device* pwm, struct pwm_state* state);

// Provider API.
int pwmchip_add(struct pwm_chip* chip);
void pwmchip_remove(struct pwm_chip* chip);
int devm_pwmchip_add(struct device* dev, struct pwm_chip* chip);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_PWM_H_
