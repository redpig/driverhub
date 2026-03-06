// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_PRINT_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_PRINT_H_

// KMI shims for Linux kernel logging APIs.
//
// Provides: printk, pr_err, pr_warn, pr_info, pr_debug,
//           dev_err, dev_warn, dev_info, dev_dbg
//
// All map to Fuchsia FX_LOG macros.

#ifdef __cplusplus
extern "C" {
#endif

#define KERN_EMERG   "<0>"
#define KERN_ALERT   "<1>"
#define KERN_CRIT    "<2>"
#define KERN_ERR     "<3>"
#define KERN_WARNING "<4>"
#define KERN_NOTICE  "<5>"
#define KERN_INFO    "<6>"
#define KERN_DEBUG   "<7>"

int printk(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#define pr_err(fmt, ...)    printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)   printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)   printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)  printk(KERN_DEBUG fmt, ##__VA_ARGS__)

struct device;

void dev_err(const struct device* dev, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
void dev_warn(const struct device* dev, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
void dev_info(const struct device* dev, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
void dev_dbg(const struct device* dev, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_PRINT_H_
