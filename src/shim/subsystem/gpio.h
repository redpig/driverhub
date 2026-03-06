// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_GPIO_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_GPIO_H_

// KMI shims for the Linux GPIO subsystem APIs.
//
// Provides: gpio_request, gpio_free, gpio_direction_input,
//           gpio_direction_output, gpio_get_value, gpio_set_value,
//           gpio_to_irq, devm_gpio_request, devm_gpio_request_one,
//           gpiod_get, gpiod_put, gpiod_direction_input, etc.
//
// On Fuchsia: maps to fuchsia.hardware.gpio/Gpio FIDL protocol.

#ifdef __cplusplus
extern "C" {
#endif

struct device;

// Legacy integer-based GPIO API.
int gpio_request(unsigned gpio, const char *label);
void gpio_free(unsigned gpio);
int gpio_direction_input(unsigned gpio);
int gpio_direction_output(unsigned gpio, int value);
int gpio_get_value(unsigned gpio);
void gpio_set_value(unsigned gpio, int value);
int gpio_to_irq(unsigned gpio);
int gpio_is_valid(int number);

// Device-managed GPIO request.
int devm_gpio_request(struct device *dev, unsigned gpio, const char *label);
int devm_gpio_request_one(struct device *dev, unsigned gpio,
                          unsigned long flags, const char *label);

// GPIO flags for devm_gpio_request_one.
#define GPIOF_DIR_OUT   (0 << 0)
#define GPIOF_DIR_IN    (1 << 0)
#define GPIOF_INIT_LOW  (0 << 1)
#define GPIOF_INIT_HIGH (1 << 1)
#define GPIOF_OUT_INIT_LOW   (GPIOF_DIR_OUT | GPIOF_INIT_LOW)
#define GPIOF_OUT_INIT_HIGH  (GPIOF_DIR_OUT | GPIOF_INIT_HIGH)
#define GPIOF_IN             GPIOF_DIR_IN

// GPIO descriptor API (newer, preferred over integer-based).
struct gpio_desc;

struct gpio_desc *gpiod_get(struct device *dev, const char *con_id,
                            unsigned long flags);
struct gpio_desc *gpiod_get_optional(struct device *dev, const char *con_id,
                                     unsigned long flags);
struct gpio_desc *devm_gpiod_get(struct device *dev, const char *con_id,
                                 unsigned long flags);
struct gpio_desc *devm_gpiod_get_optional(struct device *dev,
                                          const char *con_id,
                                          unsigned long flags);
void gpiod_put(struct gpio_desc *desc);

int gpiod_direction_input(struct gpio_desc *desc);
int gpiod_direction_output(struct gpio_desc *desc, int value);
int gpiod_get_value(const struct gpio_desc *desc);
void gpiod_set_value(struct gpio_desc *desc, int value);
int gpiod_to_irq(const struct gpio_desc *desc);

// GPIO descriptor flags.
#define GPIOD_ASIS        0
#define GPIOD_IN          1
#define GPIOD_OUT_LOW     2
#define GPIOD_OUT_HIGH    3

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_GPIO_H_
