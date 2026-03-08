// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// LED subsystem shim.
//
// In userspace, LED state is tracked but no hardware is controlled.
// On Fuchsia: maps to fuchsia.hardware.light FIDL.

#include "src/shim/subsystem/led.h"

#include <cstdio>
#include <cstdlib>

extern "C" {

int led_classdev_register(struct device* /*parent*/, struct led_classdev* led) {
  if (!led) return -22;
  if (!led->max_brightness)
    led->max_brightness = LED_FULL;
  fprintf(stderr, "driverhub: led: registered '%s' (max_brightness=%d)\n",
          led->name ? led->name : "unknown", led->max_brightness);
  return 0;
}

int devm_led_classdev_register(struct device* parent,
                                struct led_classdev* led) {
  return led_classdev_register(parent, led);
}

int devm_led_classdev_register_ext(struct device* parent,
                                    struct led_classdev* led,
                                    struct led_init_data* /*init_data*/) {
  return led_classdev_register(parent, led);
}

void led_classdev_unregister(struct led_classdev* led) {
  if (!led) return;
  fprintf(stderr, "driverhub: led: unregistered '%s'\n",
          led->name ? led->name : "unknown");
}

void led_set_brightness(struct led_classdev* led, int brightness) {
  if (!led) return;
  if (brightness > led->max_brightness)
    brightness = led->max_brightness;
  if (brightness < 0)
    brightness = 0;
  led->brightness = brightness;
  if (led->brightness_set)
    led->brightness_set(led, brightness);
  else if (led->brightness_set_blocking)
    led->brightness_set_blocking(led, brightness);
}

int led_update_brightness(struct led_classdev* led) {
  if (!led) return -22;
  if (led->brightness_get)
    led->brightness = led->brightness_get(led);
  return led->brightness;
}

void led_blink_set(struct led_classdev* led,
                   unsigned long* delay_on, unsigned long* delay_off) {
  if (!led) return;
  if (led->blink_set)
    led->blink_set(led, delay_on, delay_off);
}

void led_blink_set_oneshot(struct led_classdev* led,
                           unsigned long* delay_on, unsigned long* delay_off,
                           int /*invert*/) {
  led_blink_set(led, delay_on, delay_off);
}

void led_trigger_event(void* /*trigger*/, int /*brightness*/) {
  // No-op.
}

void led_trigger_blink(void* /*trigger*/,
                       unsigned long /*delay_on*/, unsigned long /*delay_off*/) {
  // No-op.
}

int led_trigger_register(struct led_trigger* trigger) {
  if (!trigger) return -22;
  fprintf(stderr, "driverhub: led: trigger registered '%s'\n",
          trigger->name ? trigger->name : "unknown");
  return 0;
}

void led_trigger_unregister(struct led_trigger* trigger) {
  if (!trigger) return;
  fprintf(stderr, "driverhub: led: trigger unregistered '%s'\n",
          trigger->name ? trigger->name : "unknown");
}

}  // extern "C"
