// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _DRIVERHUB_LINUX_GPIO_H
#define _DRIVERHUB_LINUX_GPIO_H

// Legacy integer-based GPIO consumer API.
// For GPIO controller (provider) drivers, use <linux/gpio/driver.h>.

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct device;
struct gpio_desc;

// Integer-based GPIO API.
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

// GPIO flags.
#define GPIOF_DIR_OUT       (0 << 0)
#define GPIOF_DIR_IN        (1 << 0)
#define GPIOF_INIT_LOW      (0 << 1)
#define GPIOF_INIT_HIGH     (1 << 1)
#define GPIOF_OUT_INIT_LOW  (GPIOF_DIR_OUT | GPIOF_INIT_LOW)
#define GPIOF_OUT_INIT_HIGH (GPIOF_DIR_OUT | GPIOF_INIT_HIGH)
#define GPIOF_IN            GPIOF_DIR_IN

// Descriptor-based GPIO API.
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

#define GPIOD_ASIS      0
#define GPIOD_IN        1
#define GPIOD_OUT_LOW   2
#define GPIOD_OUT_HIGH  3

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* _DRIVERHUB_LINUX_GPIO_H */
