/* SPDX-License-Identifier: GPL-2.0 */
/* DriverHub shim: linux/module.h */

#ifndef _DRIVERHUB_LINUX_MODULE_H
#define _DRIVERHUB_LINUX_MODULE_H

#include <linux/types.h>

/* printk */
int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define KERN_EMERG   "<0>"
#define KERN_ALERT   "<1>"
#define KERN_CRIT    "<2>"
#define KERN_ERR     "<3>"
#define KERN_WARNING "<4>"
#define KERN_NOTICE  "<5>"
#define KERN_INFO    "<6>"
#define KERN_DEBUG   "<7>"

#define pr_err(fmt, ...)    printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)   printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)   printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)  printk(KERN_DEBUG fmt, ##__VA_ARGS__)

/* __init / __exit annotations — no-ops in userspace. */
#define __init
#define __exit

/* GFP flags */
typedef unsigned int gfp_t;
#define GFP_KERNEL  0x0u
#define GFP_ATOMIC  0x1u

/* Memory */
void *kmalloc(size_t size, gfp_t flags);
void *kzalloc(size_t size, gfp_t flags);
void kfree(const void *p);

/* Error codes */
#define ENOMEM   12
#define EINVAL   22
#define ENODEV   19
#define EBUSY    16

/* Module metadata macros — place strings in .modinfo section. */
#define ___PASTE(a, b) a##b
#define __PASTE(a, b) ___PASTE(a, b)
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__unique_id_, prefix), __COUNTER__)

#define MODULE_INFO(tag, info) \
	static const char __UNIQUE_ID(modinfo)[] \
		__attribute__((section(".modinfo"), used)) = #tag "=" info

#define MODULE_LICENSE(_license)    MODULE_INFO(license, _license)
#define MODULE_AUTHOR(_author)      MODULE_INFO(author, _author)
#define MODULE_DESCRIPTION(_desc)   MODULE_INFO(description, _desc)
#define MODULE_ALIAS(_alias)        MODULE_INFO(alias, _alias)

/* module_init / module_exit */
#define module_init(fn) \
	int init_module(void) __attribute__((alias(#fn)))
#define module_exit(fn) \
	void cleanup_module(void) __attribute__((alias(#fn)))

/* EXPORT_SYMBOL */
void __driverhub_export_symbol(const char *name, void *addr);

#define EXPORT_SYMBOL(sym) \
	static void __attribute__((constructor)) __export_##sym(void) { \
		__driverhub_export_symbol(#sym, (void *)&sym); \
	}
#define EXPORT_SYMBOL_GPL(sym) EXPORT_SYMBOL(sym)

#endif /* _DRIVERHUB_LINUX_MODULE_H */
