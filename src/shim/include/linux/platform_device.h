/* SPDX-License-Identifier: GPL-2.0 */
/* DriverHub shim: linux/platform_device.h */

#ifndef _DRIVERHUB_LINUX_PLATFORM_DEVICE_H
#define _DRIVERHUB_LINUX_PLATFORM_DEVICE_H

#include <linux/device.h>

#define IORESOURCE_MEM  0x00000200
#define IORESOURCE_IRQ  0x00000400

struct resource {
	u64 start;
	u64 end;
	const char *name;
	unsigned long flags;
};

struct platform_device {
	const char *name;
	int id;
	struct device dev;
	unsigned int num_resources;
	struct resource *resource;
};

struct of_device_id {
	char compatible[128];
	const void *data;
};

struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	void (*shutdown)(struct platform_device *);
	struct {
		const char *name;
		const struct of_device_id *of_match_table;
	} driver;
};

int platform_driver_register(struct platform_driver *drv);
void platform_driver_unregister(struct platform_driver *drv);

struct platform_device *platform_device_alloc(const char *name, int id);
int platform_device_add(struct platform_device *pdev);
void platform_device_del(struct platform_device *pdev);
void platform_device_put(struct platform_device *pdev);
void platform_device_unregister(struct platform_device *pdev);

struct resource *platform_get_resource(struct platform_device *pdev,
				       unsigned int type, unsigned int num);
int platform_get_irq(struct platform_device *pdev, unsigned int num);

static inline void *platform_get_drvdata(const struct platform_device *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}

static inline void platform_set_drvdata(struct platform_device *pdev,
					void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}

#endif /* _DRIVERHUB_LINUX_PLATFORM_DEVICE_H */
