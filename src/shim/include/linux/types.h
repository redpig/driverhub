/* SPDX-License-Identifier: GPL-2.0 */
/* DriverHub shim: linux/types.h */

#ifndef _DRIVERHUB_LINUX_TYPES_H
#define _DRIVERHUB_LINUX_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef s64 time64_t;
typedef s64 ktime_t;

#define U32_MAX ((u32)~0U)
#define U64_MAX ((u64)~0ULL)

#endif /* _DRIVERHUB_LINUX_TYPES_H */
