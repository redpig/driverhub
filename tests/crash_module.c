// Test module that deliberately crashes during init.
// Used to verify process isolation: the runner and sibling modules should
// survive this module's crash.

#include <linux/module.h>

static int __init crash_init(void)
{
	printk(KERN_INFO "crash_module: about to crash\n");

	// Trigger a null pointer dereference.
	int *p = NULL;
	*p = 42;

	// Should never reach here.
	printk(KERN_ERR "crash_module: survived crash?!\n");
	return 0;
}

static void __exit crash_exit(void)
{
	printk(KERN_INFO "crash_module: exit (should not be called)\n");
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Crash test module for isolation verification");

module_init(crash_init);
module_exit(crash_exit);
