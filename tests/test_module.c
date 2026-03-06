// Minimal test kernel module for validating the DriverHub loader.
// This is compiled as a relocatable object (ET_REL) to mimic a .ko file.

// These symbols will be resolved against the KMI shim library at load time.
extern int printk(const char* fmt, ...);
extern void* kmalloc(unsigned long size, unsigned int flags);
extern void kfree(const void* p);

static int test_init(void) {
  printk("<6>test_module: init called\n");
  void* p = kmalloc(64, 0);
  if (p) {
    printk("<6>test_module: kmalloc(64) = %p\n", p);
    kfree(p);
    printk("<6>test_module: kfree ok\n");
  }
  return 0;
}

static void test_exit(void) {
  printk("<6>test_module: exit called\n");
}

// These are the entry points the loader looks for.
int init_module(void) __attribute__((alias("test_init")));
void cleanup_module(void) __attribute__((alias("test_exit")));

// Modinfo section entries.
static const char __mod_license[]
    __attribute__((section(".modinfo"), used)) = "license=GPL";
static const char __mod_description[]
    __attribute__((section(".modinfo"), used)) = "description=DriverHub test module";
static const char __mod_author[]
    __attribute__((section(".modinfo"), used)) = "author=DriverHub";
