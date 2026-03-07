// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PWM subsystem shim.
//
// In userspace, PWM devices are tracked with simulated state.
// On Fuchsia: maps to fuchsia.hardware.pwm FIDL.

#include "src/shim/subsystem/pwm.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {

struct pwm_device* pwm_request(int pwm, const char* label) {
  auto* dev = static_cast<struct pwm_device*>(
      calloc(1, sizeof(struct pwm_device)));
  if (!dev) return nullptr;
  dev->hwpwm = pwm;
  dev->label = label;
  dev->state.period = 1000000;  // 1ms default.
  fprintf(stderr, "driverhub: pwm: request pwm%d '%s'\n", pwm,
          label ? label : "");
  return dev;
}

struct pwm_device* pwm_get(struct device* /*dev*/, const char* con_id) {
  return pwm_request(0, con_id);
}

struct pwm_device* devm_pwm_get(struct device* dev, const char* con_id) {
  return pwm_get(dev, con_id);
}

void pwm_free(struct pwm_device* pwm) {
  if (!pwm) return;
  fprintf(stderr, "driverhub: pwm: free pwm%u\n", pwm->hwpwm);
  free(pwm);
}

void pwm_put(struct pwm_device* pwm) {
  pwm_free(pwm);
}

int pwm_config(struct pwm_device* pwm, int duty_ns, int period_ns) {
  if (!pwm) return -22;
  pwm->state.duty_cycle = duty_ns;
  pwm->state.period = period_ns;
  return 0;
}

int pwm_set_polarity(struct pwm_device* pwm, enum pwm_polarity polarity) {
  if (!pwm) return -22;
  pwm->state.polarity = polarity;
  return 0;
}

int pwm_enable(struct pwm_device* pwm) {
  if (!pwm) return -22;
  pwm->state.enabled = 1;
  fprintf(stderr, "driverhub: pwm: enable pwm%u (duty=%luns, period=%luns)\n",
          pwm->hwpwm,
          (unsigned long)pwm->state.duty_cycle,
          (unsigned long)pwm->state.period);
  return 0;
}

void pwm_disable(struct pwm_device* pwm) {
  if (!pwm) return;
  pwm->state.enabled = 0;
}

int pwm_apply_state(struct pwm_device* pwm, const struct pwm_state* state) {
  if (!pwm || !state) return -22;
  pwm->state = *state;
  if (state->enabled) {
    fprintf(stderr, "driverhub: pwm: apply pwm%u (duty=%luns, period=%luns)\n",
            pwm->hwpwm,
            (unsigned long)state->duty_cycle,
            (unsigned long)state->period);
  }
  return 0;
}

void pwm_get_state(const struct pwm_device* pwm, struct pwm_state* state) {
  if (!pwm || !state) return;
  *state = pwm->state;
}

int pwmchip_add(struct pwm_chip* chip) {
  if (!chip) return -22;
  if (chip->npwm > 0) {
    chip->pwms = static_cast<struct pwm_device*>(
        calloc(chip->npwm, sizeof(struct pwm_device)));
    for (unsigned i = 0; i < chip->npwm; i++) {
      chip->pwms[i].hwpwm = i;
      chip->pwms[i].chip = chip;
    }
  }
  fprintf(stderr, "driverhub: pwm: registered chip (%u channels)\n",
          chip->npwm);
  return 0;
}

void pwmchip_remove(struct pwm_chip* chip) {
  if (!chip) return;
  fprintf(stderr, "driverhub: pwm: unregistered chip\n");
  free(chip->pwms);
  chip->pwms = nullptr;
}

int devm_pwmchip_add(struct device* /*dev*/, struct pwm_chip* chip) {
  return pwmchip_add(chip);
}

}  // extern "C"
