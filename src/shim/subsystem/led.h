// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_LED_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_LED_H_

// LED subsystem shim: notification LEDs, charging indicators, flash LEDs.
//
// Provides:
//   led_classdev_register, devm_led_classdev_register
//   led_classdev_unregister
//   led_set_brightness, led_update_brightness
//   devm_led_classdev_register_ext
//
// On Fuchsia: maps to fuchsia.hardware.light or stubs.

#include <cstdint>

struct device;

#ifdef __cplusplus
extern "C" {
#endif

#define LED_OFF    0
#define LED_HALF   127
#define LED_FULL   255

#define LED_CORE   0x0001
#define LED_BLINK  0x0002

struct led_classdev {
  const char* name;
  int brightness;
  int max_brightness;
  unsigned long flags;
  void (*brightness_set)(struct led_classdev*, int);
  int (*brightness_set_blocking)(struct led_classdev*, int);
  int (*brightness_get)(struct led_classdev*);
  int (*blink_set)(struct led_classdev*,
                   unsigned long* delay_on, unsigned long* delay_off);
  struct device* dev;
  void* trigger_data;
  const char* default_trigger;
};

struct led_init_data {
  const char* default_label;
  const char* devicename;
  void* fwnode;
};

int led_classdev_register(struct device* parent, struct led_classdev* led);
int devm_led_classdev_register(struct device* parent,
                                struct led_classdev* led);
int devm_led_classdev_register_ext(struct device* parent,
                                    struct led_classdev* led,
                                    struct led_init_data* init_data);
void led_classdev_unregister(struct led_classdev* led);

void led_set_brightness(struct led_classdev* led, int brightness);
int led_update_brightness(struct led_classdev* led);
void led_blink_set(struct led_classdev* led,
                   unsigned long* delay_on, unsigned long* delay_off);
void led_blink_set_oneshot(struct led_classdev* led,
                           unsigned long* delay_on, unsigned long* delay_off,
                           int invert);

// LED triggers.
struct led_trigger {
  const char* name;
};

void led_trigger_event(void* trigger, int brightness);
void led_trigger_blink(void* trigger,
                       unsigned long delay_on, unsigned long delay_off);
int led_trigger_register(struct led_trigger* trigger);
void led_trigger_unregister(struct led_trigger* trigger);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_LED_H_
