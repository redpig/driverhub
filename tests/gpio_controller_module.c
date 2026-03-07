// GPIO controller .ko module for DriverHub bootfs testing.
//
// Simulates a SoC GPIO controller (like Qualcomm TLMM or MediaTek GPIO)
// that registers a gpio_chip with 32 pins. Once loaded via DriverHub,
// these pins become available as fuchsia.hardware.gpio/Gpio FIDL services
// that Fuchsia DFv2 composite nodes can bind against.
//
// For example, an I2C touch controller driver might require:
//   - An I2C bus fragment
//   - A GPIO fragment for its interrupt pin
//   - A GPIO fragment for its reset pin
//
// This module provides the GPIO fragments.

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/driver.h>

#define GPIO_CTRL_NPINS   32
#define GPIO_CTRL_NAME    "driverhub-gpio"

struct gpio_ctrl_data {
	struct gpio_chip gc;
	int pin_values[GPIO_CTRL_NPINS];
	int pin_directions[GPIO_CTRL_NPINS]; // 0 = output, 1 = input
};

static struct platform_device *gpio_pdev;

// --- gpio_chip callbacks ---

static int gpio_ctrl_get_direction(struct gpio_chip *gc, unsigned offset)
{
	struct gpio_ctrl_data *d = gpiochip_get_data(gc);
	if (offset >= GPIO_CTRL_NPINS) return -22; // -EINVAL
	return d->pin_directions[offset]; // 0=out, 1=in
}

static int gpio_ctrl_direction_input(struct gpio_chip *gc, unsigned offset)
{
	struct gpio_ctrl_data *d = gpiochip_get_data(gc);
	if (offset >= GPIO_CTRL_NPINS) return -22;
	d->pin_directions[offset] = 1;
	printk(KERN_INFO "%s: pin %u → input\n", GPIO_CTRL_NAME, offset);
	return 0;
}

static int gpio_ctrl_direction_output(struct gpio_chip *gc, unsigned offset,
				      int value)
{
	struct gpio_ctrl_data *d = gpiochip_get_data(gc);
	if (offset >= GPIO_CTRL_NPINS) return -22;
	d->pin_directions[offset] = 0;
	d->pin_values[offset] = value ? 1 : 0;
	printk(KERN_INFO "%s: pin %u → output (value=%d)\n",
	       GPIO_CTRL_NAME, offset, d->pin_values[offset]);
	return 0;
}

static int gpio_ctrl_get(struct gpio_chip *gc, unsigned offset)
{
	struct gpio_ctrl_data *d = gpiochip_get_data(gc);
	if (offset >= GPIO_CTRL_NPINS) return -22;
	return d->pin_values[offset];
}

static void gpio_ctrl_set(struct gpio_chip *gc, unsigned offset, int value)
{
	struct gpio_ctrl_data *d = gpiochip_get_data(gc);
	if (offset >= GPIO_CTRL_NPINS) return;
	d->pin_values[offset] = value ? 1 : 0;
}

static int gpio_ctrl_to_irq(struct gpio_chip *gc, unsigned offset)
{
	// Map GPIO offset to a virtual IRQ number.
	return (int)(offset + 256);
}

// --- Platform driver ---

static int gpio_ctrl_probe(struct platform_device *pdev)
{
	struct gpio_ctrl_data *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -12; // -ENOMEM

	// Initialize all pins as inputs.
	for (int i = 0; i < GPIO_CTRL_NPINS; i++)
		data->pin_directions[i] = 1;

	data->gc.label = GPIO_CTRL_NAME;
	data->gc.parent = &pdev->dev;
	data->gc.base = -1; // Auto-assign base
	data->gc.ngpio = GPIO_CTRL_NPINS;
	data->gc.get_direction = gpio_ctrl_get_direction;
	data->gc.direction_input = gpio_ctrl_direction_input;
	data->gc.direction_output = gpio_ctrl_direction_output;
	data->gc.get = gpio_ctrl_get;
	data->gc.set = gpio_ctrl_set;
	data->gc.to_irq = gpio_ctrl_to_irq;

	ret = devm_gpiochip_add_data(&pdev->dev, &data->gc, data);
	if (ret) {
		printk(KERN_ERR "%s: failed to register gpio chip: %d\n",
		       GPIO_CTRL_NAME, ret);
		return ret;
	}

	platform_set_drvdata(pdev, data);

	printk(KERN_INFO "%s: probed — %u GPIOs at base %d\n",
	       GPIO_CTRL_NAME, data->gc.ngpio, data->gc.base);
	return 0;
}

static struct platform_driver gpio_ctrl_driver = {
	.probe = gpio_ctrl_probe,
	.driver = {
		.name = GPIO_CTRL_NAME,
	},
};

static int __init gpio_ctrl_init(void)
{
	int err;

	printk(KERN_INFO "%s: init — registering GPIO controller\n",
	       GPIO_CTRL_NAME);

	err = platform_driver_register(&gpio_ctrl_driver);
	if (err)
		return err;

	gpio_pdev = platform_device_alloc(GPIO_CTRL_NAME, 0);
	if (!gpio_pdev) {
		platform_driver_unregister(&gpio_ctrl_driver);
		return -12; // -ENOMEM
	}

	err = platform_device_add(gpio_pdev);
	if (err) {
		platform_device_put(gpio_pdev);
		platform_driver_unregister(&gpio_ctrl_driver);
		return err;
	}

	return 0;
}

static void __exit gpio_ctrl_exit(void)
{
	printk(KERN_INFO "%s: exit — removing GPIO controller\n",
	       GPIO_CTRL_NAME);
	platform_device_unregister(gpio_pdev);
	platform_driver_unregister(&gpio_ctrl_driver);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DriverHub GPIO controller module for bootfs testing");

module_init(gpio_ctrl_init);
module_exit(gpio_ctrl_exit);
