// Test module that exports both function and data symbols.
// Used to verify both SymbolProxy paths:
// - Data symbols: shared via VMO mapping
// - Function symbols: proxied via FIDL SymbolProxy.Call()

#include <linux/module.h>
#include <linux/types.h>

// Exported data symbol — shared via VMO read mapping.
int shared_value = 42;
EXPORT_SYMBOL(shared_value);

// Exported function symbol — proxied via SymbolProxy for cross-process.
int exported_compute(int x)
{
	int result = x * shared_value;
	printk(KERN_INFO "data_export: exported_compute(%d) = %d "
	       "(shared_value=%d)\n", x, result, shared_value);
	return result;
}
EXPORT_SYMBOL(exported_compute);

// Another exported function to test multi-symbol registration.
void exported_set_value(int v)
{
	printk(KERN_INFO "data_export: exported_set_value(%d)\n", v);
	shared_value = v;
}
EXPORT_SYMBOL(exported_set_value);

static int __init data_export_init(void)
{
	printk(KERN_INFO "data_export: init (shared_value=%d)\n", shared_value);
	return 0;
}

static void __exit data_export_exit(void)
{
	printk(KERN_INFO "data_export: exit\n");
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Test module exporting function and data symbols");

module_init(data_export_init);
module_exit(data_export_exit);
