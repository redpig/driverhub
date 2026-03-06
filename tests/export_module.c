// Test module A: exports a symbol for intermodule resolution testing.
// Module B (import_module.c) will reference this symbol.

#include <linux/module.h>
#include <linux/types.h>

static int shared_counter = 0;

// A function exported for other modules to call.
int exported_add(int a, int b) {
	shared_counter++;
	printk(KERN_INFO "export_module: exported_add(%d, %d) = %d (call #%d)\n",
	       a, b, a + b, shared_counter);
	return a + b;
}

EXPORT_SYMBOL(exported_add);

static int __init export_init(void) {
	printk(KERN_INFO "export_module: init (exporting exported_add)\n");
	return 0;
}

static void __exit export_exit(void) {
	printk(KERN_INFO "export_module: exit (total calls: %d)\n", shared_counter);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Test module that exports a symbol");

module_init(export_init);
module_exit(export_exit);
