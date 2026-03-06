/* SPDX-License-Identifier: GPL-2.0 */
/* DriverHub ABI-compatible struct definitions for Linux 5.15 x86_64.
 *
 * These structs are sized and laid out to match the real kernel's ABI
 * so that pre-built .ko modules compiled against the real kernel headers
 * can be loaded and their field accesses resolve correctly.
 *
 * Only fields that shimmed code actually reads/writes are named.
 * Everything else is opaque padding.
 *
 * Generated from kernel 5.15 x86_64 defconfig:
 *   struct device:           728 bytes
 *   struct platform_device:  808 bytes
 *   struct platform_driver:  200 bytes
 *   struct rtc_device:      1264 bytes
 *   struct timer_list:        40 bytes
 *   struct rtc_class_ops:     72 bytes
 */

#ifndef _DRIVERHUB_ABI_STRUCTS_H
#define _DRIVERHUB_ABI_STRUCTS_H

#include <stdint.h>
#include <stddef.h>

/* Use static_assert in C++ mode, ABI_STATIC_ASSERT in C mode. */
#ifdef __cplusplus
#define ABI_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#else
#define ABI_STATIC_ASSERT(expr, msg) ABI_STATIC_ASSERT(expr, msg)
#endif

/* ── struct device (728 bytes) ────────────────────────────── */
/* Key offsets:
 *   init_name     @ 80
 *   parent        @ 64
 *   platform_data @ 112
 *   driver_data   @ 120
 */
struct device {
	char _pad0[64];              /* 0..63   */
	struct device *parent;       /* 64..71  */
	char _pad1[8];               /* 72..79  */
	const char *init_name;       /* 80..87  */
	char _pad2[24];              /* 88..111 */
	void *platform_data;         /* 112..119 */
	void *driver_data;           /* 120..127 */
	char _pad3[600];             /* 128..727 */
};

ABI_STATIC_ASSERT(sizeof(struct device) == 728, "device size mismatch");
ABI_STATIC_ASSERT(__builtin_offsetof(struct device, parent) == 64, "device.parent offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct device, init_name) == 80, "device.init_name offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct device, platform_data) == 112, "device.platform_data offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct device, driver_data) == 120, "device.driver_data offset");

/* ── struct platform_device (808 bytes) ───────────────────── */
/* Key offsets:
 *   name          @ 0
 *   id            @ 8
 *   dev           @ 16
 *   num_resources @ 768
 *   resource      @ 776
 */
struct resource {
	uint64_t start;
	uint64_t end;
	const char *name;
	unsigned long flags;
	/* real struct resource is larger but we only use these fields */
};

struct platform_device {
	const char *name;            /* 0..7   */
	int id;                      /* 8..11  */
	char _pad0[4];               /* 12..15 (alignment) */
	struct device dev;           /* 16..743 */
	char _pad1[24];              /* 744..767 */
	unsigned int num_resources;  /* 768..771 */
	char _pad2[4];               /* 772..775 */
	struct resource *resource;   /* 776..783 */
	char _pad3[24];              /* 784..807 */
};

ABI_STATIC_ASSERT(sizeof(struct platform_device) == 808, "platform_device size mismatch");
ABI_STATIC_ASSERT(__builtin_offsetof(struct platform_device, name) == 0, "pdev.name offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct platform_device, id) == 8, "pdev.id offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct platform_device, dev) == 16, "pdev.dev offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct platform_device, num_resources) == 768, "pdev.num_resources offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct platform_device, resource) == 776, "pdev.resource offset");

/* ── struct platform_driver (200 bytes) ───────────────────── */
/* Key offsets:
 *   probe    @ 0
 *   remove   @ 8
 *   shutdown @ 16
 *   driver   @ 40   (struct device_driver, we only need .name)
 */
struct platform_driver {
	int (*probe)(struct platform_device *);        /* 0..7   */
	int (*remove)(struct platform_device *);       /* 8..15  */
	void (*shutdown)(struct platform_device *);    /* 16..23 */
	char _pad0[16];                                /* 24..39 */
	/* Embedded struct device_driver starts at 40. */
	/* device_driver.name is at offset 0 within it. */
	const char *driver_name;                       /* 40..47 */
	char _pad1[152];                               /* 48..199 */
};

ABI_STATIC_ASSERT(sizeof(struct platform_driver) == 200, "platform_driver size mismatch");
ABI_STATIC_ASSERT(__builtin_offsetof(struct platform_driver, probe) == 0, "pdrv.probe offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct platform_driver, remove) == 8, "pdrv.remove offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct platform_driver, shutdown) == 16, "pdrv.shutdown offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct platform_driver, driver_name) == 40, "pdrv.driver.name offset");

/* ── struct timer_list (40 bytes) ─────────────────────────── */
/* Key offsets:
 *   entry    @ 0  (list_head, 16 bytes — opaque)
 *   expires  @ 16
 *   function @ 24
 *   flags    @ 32
 */
struct timer_list {
	char _pad0[16];              /* 0..15  (list_head) */
	unsigned long expires;       /* 16..23 */
	void (*function)(struct timer_list *);  /* 24..31 */
	unsigned int flags;          /* 32..35 */
	char _pad1[4];               /* 36..39 */
};

ABI_STATIC_ASSERT(sizeof(struct timer_list) == 40, "timer_list size mismatch");
ABI_STATIC_ASSERT(__builtin_offsetof(struct timer_list, expires) == 16, "timer.expires offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct timer_list, function) == 24, "timer.function offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct timer_list, flags) == 32, "timer.flags offset");

/* ── struct rtc_time (36 bytes) ───────────────────────────── */
struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

/* ── struct rtc_wkalrm (40 bytes) ─────────────────────────── */
struct rtc_wkalrm {
	unsigned char enabled;
	unsigned char pending;
	char _pad[2];
	struct rtc_time time;
};

ABI_STATIC_ASSERT(sizeof(struct rtc_wkalrm) == 40, "rtc_wkalrm size mismatch");

/* ── struct rtc_class_ops (72 bytes) ──────────────────────── */
/* Key offsets:
 *   read_time        @ 8
 *   set_time         @ 16
 *   read_alarm       @ 24
 *   set_alarm        @ 32
 *   alarm_irq_enable @ 48
 */
struct rtc_class_ops {
	void *_pad0;                 /* 0..7   (ioctl) */
	int (*read_time)(struct device *, struct rtc_time *);         /* 8..15  */
	int (*set_time)(struct device *, struct rtc_time *);          /* 16..23 */
	int (*read_alarm)(struct device *, struct rtc_wkalrm *);      /* 24..31 */
	int (*set_alarm)(struct device *, struct rtc_wkalrm *);       /* 32..39 */
	void *_pad1;                 /* 40..47 (proc) */
	int (*alarm_irq_enable)(struct device *, unsigned int);       /* 48..55 */
	char _pad2[16];              /* 56..71 */
};

ABI_STATIC_ASSERT(sizeof(struct rtc_class_ops) == 72, "rtc_class_ops size mismatch");
ABI_STATIC_ASSERT(__builtin_offsetof(struct rtc_class_ops, read_time) == 8, "ops.read_time offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct rtc_class_ops, set_time) == 16, "ops.set_time offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct rtc_class_ops, read_alarm) == 24, "ops.read_alarm offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct rtc_class_ops, set_alarm) == 32, "ops.set_alarm offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct rtc_class_ops, alarm_irq_enable) == 48, "ops.alarm_irq_enable offset");

/* ── struct rtc_device (1264 bytes) ───────────────────────── */
/* Key offsets:
 *   dev  @ 0
 *   id   @ 736
 *   ops  @ 744
 */
struct rtc_device {
	struct device dev;           /* 0..727 */
	char _pad0[8];               /* 728..735 */
	int id;                      /* 736..739 */
	char _pad1[4];               /* 740..743 */
	const struct rtc_class_ops *ops;  /* 744..751 */
	char _pad2[512];             /* 752..1263 */
};

ABI_STATIC_ASSERT(sizeof(struct rtc_device) == 1264, "rtc_device size mismatch");
ABI_STATIC_ASSERT(__builtin_offsetof(struct rtc_device, dev) == 0, "rtc.dev offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct rtc_device, id) == 736, "rtc.id offset");
ABI_STATIC_ASSERT(__builtin_offsetof(struct rtc_device, ops) == 744, "rtc.ops offset");

/* ── Inline accessors ────────────────────────────────────── */

static inline void *dev_get_drvdata(const struct device *dev)
{
	return dev->driver_data;
}

static inline void dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data;
}

static inline const char *dev_name(const struct device *dev)
{
	return dev->init_name ? dev->init_name : "(unknown)";
}

static inline void *platform_get_drvdata(const struct platform_device *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}

static inline void platform_set_drvdata(struct platform_device *pdev,
					void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}

#endif /* _DRIVERHUB_ABI_STRUCTS_H */
