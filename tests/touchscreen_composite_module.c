// Touchscreen composite driver .ko module for DriverHub testing.
//
// Simulates a touchscreen controller (like Goodix GT9xx or Focaltech FT5x06)
// that binds to a composite node requiring:
//   - GPIO pin 3 for interrupt (IRQ)
//   - GPIO pin 8 for reset
//
// On Fuchsia, this module's DFv2 child node would include bind rules matching:
//   fuchsia.hardware.gpio.Service == "driverhub/gpio-3"  (IRQ fragment)
//   fuchsia.hardware.gpio.Service == "driverhub/gpio-8"  (reset fragment)
//   fuchsia.hardware.i2c.Service  == "driverhub/i2c-touch" (data bus)
//
// This module proves that a consumer driver can use GPIO pins exposed by
// a GPIO controller .ko module through the DriverHub service bridge.

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#define TS_NAME       "dh-touchscreen"
#define TS_IRQ_PIN    3
#define TS_RESET_PIN  8

struct ts_data {
	int irq_gpio;
	int reset_gpio;
	int irq_num;
	int probe_status;   // 0 = success
};

static struct platform_device *ts_pdev;
static struct ts_data *ts_priv;

static int ts_probe(struct platform_device *pdev)
{
	struct ts_data *ts;
	int ret;

	ts = devm_kzalloc(&pdev->dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -12; // -ENOMEM

	ts->irq_gpio = TS_IRQ_PIN;
	ts->reset_gpio = TS_RESET_PIN;

	printk(KERN_INFO "%s: probe — requesting GPIO pins\n", TS_NAME);

	// --- Step 1: Request IRQ GPIO ---
	ret = gpio_request(ts->irq_gpio, "ts-irq");
	if (ret) {
		printk(KERN_ERR "%s: failed to request IRQ GPIO %d: %d\n",
		       TS_NAME, ts->irq_gpio, ret);
		ts->probe_status = ret;
		platform_set_drvdata(pdev, ts);
		ts_priv = ts;
		return ret;
	}

	ret = gpio_direction_input(ts->irq_gpio);
	if (ret) {
		printk(KERN_ERR "%s: failed to set IRQ GPIO direction: %d\n",
		       TS_NAME, ret);
		gpio_free(ts->irq_gpio);
		ts->probe_status = ret;
		platform_set_drvdata(pdev, ts);
		ts_priv = ts;
		return ret;
	}

	ts->irq_num = gpio_to_irq(ts->irq_gpio);
	printk(KERN_INFO "%s: IRQ GPIO %d → IRQ %d\n",
	       TS_NAME, ts->irq_gpio, ts->irq_num);

	// --- Step 2: Request reset GPIO ---
	ret = gpio_request(ts->reset_gpio, "ts-reset");
	if (ret) {
		printk(KERN_ERR "%s: failed to request reset GPIO %d: %d\n",
		       TS_NAME, ts->reset_gpio, ret);
		gpio_free(ts->irq_gpio);
		ts->probe_status = ret;
		platform_set_drvdata(pdev, ts);
		ts_priv = ts;
		return ret;
	}

	// --- Step 3: Reset sequence ---
	// Assert reset (active low): drive low
	ret = gpio_direction_output(ts->reset_gpio, 0);
	if (ret) {
		printk(KERN_ERR "%s: reset assert failed: %d\n", TS_NAME, ret);
		gpio_free(ts->reset_gpio);
		gpio_free(ts->irq_gpio);
		ts->probe_status = ret;
		platform_set_drvdata(pdev, ts);
		ts_priv = ts;
		return ret;
	}

	printk(KERN_INFO "%s: reset asserted (GPIO %d = low)\n",
	       TS_NAME, ts->reset_gpio);

	// Deassert reset: drive high
	gpio_set_value(ts->reset_gpio, 1);
	printk(KERN_INFO "%s: reset deasserted (GPIO %d = high)\n",
	       TS_NAME, ts->reset_gpio);

	// --- Step 4: Verify pin states ---
	int irq_val = gpio_get_value(ts->irq_gpio);
	int rst_val = gpio_get_value(ts->reset_gpio);
	printk(KERN_INFO "%s: pin states — IRQ=%d RESET=%d\n",
	       TS_NAME, irq_val, rst_val);

	ts->probe_status = 0;
	platform_set_drvdata(pdev, ts);
	ts_priv = ts;

	printk(KERN_INFO "%s: probe complete — composite binding ready\n",
	       TS_NAME);
	return 0;
}

static struct platform_driver ts_driver = {
	.probe = ts_probe,
	.driver = {
		.name = TS_NAME,
	},
};

static int __init ts_init(void)
{
	int err;

	printk(KERN_INFO "%s: init — touchscreen composite driver\n", TS_NAME);

	err = platform_driver_register(&ts_driver);
	if (err)
		return err;

	ts_pdev = platform_device_alloc(TS_NAME, 0);
	if (!ts_pdev) {
		platform_driver_unregister(&ts_driver);
		return -12;
	}

	err = platform_device_add(ts_pdev);
	if (err) {
		platform_device_put(ts_pdev);
		platform_driver_unregister(&ts_driver);
		return err;
	}

	return 0;
}

static void __exit ts_exit(void)
{
	printk(KERN_INFO "%s: exit — removing touchscreen driver\n", TS_NAME);

	if (ts_priv) {
		if (ts_priv->probe_status == 0) {
			gpio_free(ts_priv->reset_gpio);
			gpio_free(ts_priv->irq_gpio);
		}
	}

	platform_device_unregister(ts_pdev);
	platform_driver_unregister(&ts_driver);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DriverHub touchscreen composite driver (GPIO consumer)");
MODULE_ALIAS("dh-touchscreen");

module_init(ts_init);
module_exit(ts_exit);
