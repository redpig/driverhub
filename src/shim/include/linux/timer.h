/* SPDX-License-Identifier: GPL-2.0 */
/* DriverHub shim: linux/timer.h */

#ifndef _DRIVERHUB_LINUX_TIMER_H
#define _DRIVERHUB_LINUX_TIMER_H

#include <linux/types.h>

#define HZ 1000

/* jiffies — updated by the shim time layer. */
extern volatile unsigned long jiffies;

struct timer_list {
	unsigned long expires;
	void (*function)(struct timer_list *);
	unsigned int flags;
	/* Internal: tracks whether the timer is pending. */
	int _active;
};

/* timer_setup — initialize a timer with a callback. */
static inline void timer_setup(struct timer_list *timer,
			       void (*func)(struct timer_list *),
			       unsigned int flags)
{
	timer->function = func;
	timer->expires = 0;
	timer->flags = flags;
	timer->_active = 0;
}

/* timer_container_of — recover the enclosing struct from a timer_list. */
#define timer_container_of(var, timer_ptr, member) \
	((typeof(var))((char *)(timer_ptr) - __builtin_offsetof(typeof(*var), member)))

/* add_timer — "schedule" the timer. In userspace shim, we just mark active. */
void add_timer(struct timer_list *timer);

/* del_timer — cancel the timer. Returns whether it was active. */
int del_timer(struct timer_list *timer);

/*
 * timer_delete is the modern Linux kernel name for del_timer.
 * We use a macro to avoid conflicting with POSIX timer_delete(timer_t).
 */
#define timer_delete(t) del_timer(t)

/* mod_timer — modify timer expiry. */
int mod_timer(struct timer_list *timer, unsigned long expires);

#endif /* _DRIVERHUB_LINUX_TIMER_H */
