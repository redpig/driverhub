// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _DRIVERHUB_LINUX_GPIO_DRIVER_H
#define _DRIVERHUB_LINUX_GPIO_DRIVER_H

// GPIO controller (provider) API shim.
//
// Provides: struct gpio_chip, gpiochip_add_data, gpiochip_remove,
//           devm_gpiochip_add_data.
//
// On Fuchsia: registering a gpio_chip exposes its pins through the
// fuchsia.hardware.gpio/Gpio FIDL service, making them available as
// DFv2 composite node fragments for other drivers to bind.

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct device;
struct module;

// GPIO chip — represents one GPIO controller with a bank of pins.
// This matches the essential fields from the Linux gpio_chip struct.
struct gpio_chip {
	const char		*label;
	struct device		*parent;
	struct module		*owner;
	int			base;
	unsigned int		ngpio;

	// Pin operation callbacks.
	int			(*request)(struct gpio_chip *gc, unsigned offset);
	void			(*free)(struct gpio_chip *gc, unsigned offset);
	int			(*get_direction)(struct gpio_chip *gc,
						 unsigned offset);
	int			(*direction_input)(struct gpio_chip *gc,
						   unsigned offset);
	int			(*direction_output)(struct gpio_chip *gc,
						    unsigned offset, int value);
	int			(*get)(struct gpio_chip *gc, unsigned offset);
	void			(*set)(struct gpio_chip *gc, unsigned offset,
				       int value);
	int			(*to_irq)(struct gpio_chip *gc,
					  unsigned offset);

	// Private data pointer (set via gpiochip_add_data, read via
	// gpiochip_get_data).
	void			*_data;
};

// Register a GPIO chip. Returns 0 on success.
int gpiochip_add_data(struct gpio_chip *gc, void *data);

// Unregister a GPIO chip.
void gpiochip_remove(struct gpio_chip *gc);

// Device-managed variant — chip is removed when parent device is freed.
int devm_gpiochip_add_data(struct device *dev, struct gpio_chip *gc,
			   void *data);

// Get the driver-private data stored in the chip.
static inline void *gpiochip_get_data(struct gpio_chip *gc)
{
	return gc ? gc->_data : (void *)0;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* _DRIVERHUB_LINUX_GPIO_DRIVER_H */
