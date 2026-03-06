/* SPDX-License-Identifier: GPL-2.0 */
/* DriverHub shim: linux/device.h */

#ifndef _DRIVERHUB_LINUX_DEVICE_H
#define _DRIVERHUB_LINUX_DEVICE_H

#include <linux/types.h>

struct device {
	const char *init_name;
	struct device *parent;
	void *driver_data;
	void *platform_data;
	void *devres_head;
};

static inline void *dev_get_drvdata(const struct device *dev)
{
	return dev->driver_data;
}

static inline void dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data;
}

static inline const char *dev_name(const struct device *dev)
{
	return dev->init_name ? dev->init_name : "(unknown)";
}

void *devm_kmalloc(struct device *dev, size_t size, unsigned int flags);
void *devm_kzalloc(struct device *dev, size_t size, unsigned int flags);

/* device_init_wakeup — stub for userspace (no PM). */
static inline int device_init_wakeup(struct device *dev, bool enable)
{
	(void)dev;
	(void)enable;
	return 0;
}

#endif /* _DRIVERHUB_LINUX_DEVICE_H */
