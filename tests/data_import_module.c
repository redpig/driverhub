// Test module that imports both function and data symbols from
// data_export_module. Must be loaded after data_export_module.
//
// When colocated: symbols resolve to direct in-process pointers.
// When isolated: data resolves via VMO map, functions via SymbolProxy.

#include <linux/module.h>
#include <linux/types.h>

// Imported data symbol (resolved via VMO mapping when cross-process).
extern int shared_value;

// Imported function symbols (resolved via SymbolProxy when cross-process).
extern int exported_compute(int x);
extern void exported_set_value(int v);

static int __init data_import_init(void)
{
	int result;

	printk(KERN_INFO "data_import: init\n");

	// Read imported data symbol.
	printk(KERN_INFO "data_import: shared_value = %d (expect 42)\n",
	       shared_value);

	// Call imported function symbol.
	result = exported_compute(3);
	printk(KERN_INFO "data_import: exported_compute(3) = %d (expect 126)\n",
	       result);

	// Modify via imported setter and verify.
	exported_set_value(10);
	printk(KERN_INFO "data_import: after set_value(10), shared_value = %d\n",
	       shared_value);

	result = exported_compute(5);
	printk(KERN_INFO "data_import: exported_compute(5) = %d (expect 50)\n",
	       result);

	return 0;
}

static void __exit data_import_exit(void)
{
	printk(KERN_INFO "data_import: exit\n");
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Test module importing function and data symbols");
MODULE_INFO(depends, "data_export_module");

module_init(data_import_init);
module_exit(data_import_exit);
