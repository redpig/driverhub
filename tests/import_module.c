// Test module B: imports a symbol from export_module.c via intermodule
// resolution.  Must be loaded after export_module.

#include <linux/module.h>
#include <linux/types.h>

// Declared as extern — resolved at load time from export_module's
// EXPORT_SYMBOL(exported_add).
extern int exported_add(int a, int b);

static int __init import_init(void) {
	int result;

	printk(KERN_INFO "import_module: init\n");

	result = exported_add(3, 4);
	printk(KERN_INFO "import_module: exported_add(3, 4) = %d\n", result);

	result = exported_add(10, 20);
	printk(KERN_INFO "import_module: exported_add(10, 20) = %d\n", result);

	return 0;
}

static void __exit import_exit(void) {
	printk(KERN_INFO "import_module: exit\n");
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Test module that imports a symbol from export_module");

module_init(import_init);
module_exit(import_exit);
