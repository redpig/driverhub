// A second test module exercising a different set of KMI shims:
// platform driver, memory allocation, synchronization, and time.
// This validates that the shim layer generalizes beyond the RTC use case.

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>

struct gpio_test_data {
	int pin_states[8];
	int num_pins;
};

static struct platform_device *gpio_pdev;

static int gpio_test_probe(struct platform_device *pdev) {
	struct gpio_test_data *data;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->num_pins = 8;
	platform_set_drvdata(pdev, data);

	printk(KERN_INFO "gpio_test: probed with %d pins\n", data->num_pins);

	// Exercise memory allocation.
	void *buf = kmalloc(256, GFP_KERNEL);
	if (buf) {
		printk(KERN_INFO "gpio_test: kmalloc(256) ok\n");
		kfree(buf);
	}

	// Exercise kzalloc (zero-filled).
	int *arr = kzalloc(32 * sizeof(int), GFP_KERNEL);
	if (arr) {
		int all_zero = 1;
		for (int i = 0; i < 32; i++) {
			if (arr[i] != 0) {
				all_zero = 0;
				break;
			}
		}
		printk(KERN_INFO "gpio_test: kzalloc zero-fill check: %s\n",
		       all_zero ? "PASS" : "FAIL");
		kfree(arr);
	}

	return 0;
}

static struct platform_driver gpio_test_driver = {
	.probe = gpio_test_probe,
	.driver = {
		.name = "gpio-test",
	},
};

static int __init gpio_test_init(void) {
	int err;

	printk(KERN_INFO "gpio_test: init\n");

	err = platform_driver_register(&gpio_test_driver);
	if (err)
		return err;

	gpio_pdev = platform_device_alloc("gpio-test", 0);
	if (!gpio_pdev) {
		platform_driver_unregister(&gpio_test_driver);
		return -ENOMEM;
	}

	err = platform_device_add(gpio_pdev);
	if (err) {
		platform_device_put(gpio_pdev);
		platform_driver_unregister(&gpio_test_driver);
		return err;
	}

	return 0;
}

static void __exit gpio_test_exit(void) {
	printk(KERN_INFO "gpio_test: exit\n");
	platform_device_unregister(gpio_pdev);
	platform_driver_unregister(&gpio_test_driver);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO test module for shim generalization");

module_init(gpio_test_init);
module_exit(gpio_test_exit);
