// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_MODULE_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_MODULE_H_

// KMI shims for Linux kernel module lifecycle macros.
//
// Provides: module_init, module_exit, MODULE_LICENSE, MODULE_AUTHOR,
//           MODULE_DESCRIPTION, EXPORT_SYMBOL, EXPORT_SYMBOL_GPL
//
// module_init/exit generate known symbol names that the loader resolves.
// EXPORT_SYMBOL registers symbols in the intermodule symbol registry.

#ifdef __cplusplus
extern "C" {
#endif

// Module init/exit macros. These create aliases that the module loader
// looks for when resolving entry points.
#define module_init(fn) \
  int init_module(void) __attribute__((alias(#fn)))

#define module_exit(fn) \
  void cleanup_module(void) __attribute__((alias(#fn)))

// Module metadata macros. These place strings in the .modinfo section
// where the loader extracts them.
#define MODULE_INFO(tag, info) \
  static const char __UNIQUE_ID(tag)[] \
    __attribute__((section(".modinfo"), used)) = #tag "=" info

#define MODULE_LICENSE(license)     MODULE_INFO(license, license)
#define MODULE_AUTHOR(author)       MODULE_INFO(author, author)
#define MODULE_DESCRIPTION(desc)    MODULE_INFO(description, desc)
#define MODULE_ALIAS(alias)         MODULE_INFO(alias, alias)

// Symbol export. Registers the symbol address in the DriverHub symbol
// registry so that other modules can resolve it.
void __driverhub_export_symbol(const char* name, void* addr);

#define EXPORT_SYMBOL(sym) \
  static void __attribute__((constructor)) __export_##sym(void) { \
    __driverhub_export_symbol(#sym, (void*)&sym); \
  }

#define EXPORT_SYMBOL_GPL(sym) EXPORT_SYMBOL(sym)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_MODULE_H_
