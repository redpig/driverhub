/* SPDX-License-Identifier: GPL-2.0 */
/* DriverHub shim: linux/rtc.h */

#ifndef _DRIVERHUB_LINUX_RTC_H
#define _DRIVERHUB_LINUX_RTC_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/timer.h>

/* RTC interrupt flags. */
#define RTC_IRQF  0x80
#define RTC_AF    0x20

/* RTC time structure — matches struct tm layout. */
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

/* Wake alarm structure. */
struct rtc_wkalrm {
	unsigned char enabled;
	unsigned char pending;
	struct rtc_time time;
};

struct rtc_device;

/* RTC class operations — the module fills these in. */
struct rtc_class_ops {
	int (*read_time)(struct device *, struct rtc_time *);
	int (*set_time)(struct device *, struct rtc_time *);
	int (*read_alarm)(struct device *, struct rtc_wkalrm *);
	int (*set_alarm)(struct device *, struct rtc_wkalrm *);
	int (*alarm_irq_enable)(struct device *, unsigned int);
};

/* Minimal rtc_device for the shim. */
struct rtc_device {
	struct device dev;
	const struct rtc_class_ops *ops;
	char name[32];
	int id;
};

/* RTC time conversion helpers. */
void rtc_time64_to_tm(time64_t time, struct rtc_time *tm);
time64_t rtc_tm_to_time64(struct rtc_time *tm);

/* Monotonic real-time seconds (used by rtc-test). */
time64_t ktime_get_real_seconds(void);

/* Allocate an RTC device (device-managed). */
struct rtc_device *devm_rtc_allocate_device(struct device *parent);

/* Register an RTC device (device-managed). */
int devm_rtc_register_device(struct rtc_device *rtc);

/* Report an RTC interrupt. */
void rtc_update_irq(struct rtc_device *rtc, unsigned long num,
		    unsigned long events);

#endif /* _DRIVERHUB_LINUX_RTC_H */
