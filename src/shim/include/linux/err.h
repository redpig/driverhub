/* SPDX-License-Identifier: GPL-2.0 */
/* DriverHub shim: linux/err.h */

#ifndef _DRIVERHUB_LINUX_ERR_H
#define _DRIVERHUB_LINUX_ERR_H

#include <linux/types.h>

#define MAX_ERRNO 4095

static inline void *ERR_PTR(long error)
{
	return (void *)error;
}

static inline long PTR_ERR(const void *ptr)
{
	return (long)ptr;
}

static inline int IS_ERR(const void *ptr)
{
	return (unsigned long)ptr >= (unsigned long)-MAX_ERRNO;
}

static inline int IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR(ptr);
}

#endif /* _DRIVERHUB_LINUX_ERR_H */
