// SPDX-License-Identifier: GPL-2.0
/*
 * rtc_cmos_module.c — Minimal CMOS RTC driver for MC146818.
 *
 * Reads real-time clock data from the CMOS RTC via I/O ports 0x70/0x71.
 * This is the standard PC RTC present on every x86 QEMU instance.
 *
 * Register map (MC146818 / compatible):
 *   0x00: Seconds       0x02: Minutes      0x04: Hours
 *   0x06: Day of week   0x07: Day of month 0x08: Month
 *   0x09: Year          0x0A: Status Reg A  0x0B: Status Reg B
 */

// KMI shim symbols resolved at load time.
extern int printk(const char *fmt, ...);
extern unsigned char inb(unsigned short port);
extern void outb(unsigned char val, unsigned short port);

struct resource;
extern struct resource *__request_region(struct resource *parent,
    unsigned long start, unsigned long n, const char *name, int flags);
extern void __release_region(struct resource *parent,
    unsigned long start, unsigned long n);

// MC146818 CMOS RTC I/O ports.
#define RTC_PORT_INDEX 0x70
#define RTC_PORT_DATA  0x71
#define RTC_PORT_LEN   2

// CMOS register offsets.
#define RTC_SECONDS    0x00
#define RTC_MINUTES    0x02
#define RTC_HOURS      0x04
#define RTC_DAY        0x07
#define RTC_MONTH      0x08
#define RTC_YEAR       0x09
#define RTC_STATUS_B   0x0B

// Status Register B bits.
#define RTC_B_BCD      0x04   // 0 = BCD, 1 = binary
#define RTC_B_24H      0x02   // 0 = 12h, 1 = 24h

// Read a CMOS register via port I/O.
static unsigned char cmos_read(unsigned char reg)
{
    outb(reg, RTC_PORT_INDEX);
    return inb(RTC_PORT_DATA);
}

// Convert BCD to binary.
static unsigned char bcd2bin(unsigned char val)
{
    return (val & 0x0F) + (val >> 4) * 10;
}

static struct resource *rtc_region;

static int rtc_cmos_init(void)
{
    unsigned char sec, min, hour, day, mon, year;
    unsigned char status_b;

    printk("<6>rtc-cmos: initializing MC146818 CMOS RTC driver\n");

    // Request I/O port region. On Fuchsia, this calls
    // zx_ioports_request() via the shim layer.
    rtc_region = __request_region((struct resource *)0,
                                  RTC_PORT_INDEX, RTC_PORT_LEN,
                                  "rtc-cmos", 0);
    if (!rtc_region) {
        printk("<3>rtc-cmos: failed to request I/O ports 0x70-0x71\n");
        return -1;
    }

    // Read status register B to determine BCD/binary mode.
    status_b = cmos_read(RTC_STATUS_B);
    printk("<6>rtc-cmos: Status B = 0x%02x (BCD=%s, 24h=%s)\n",
           status_b,
           (status_b & RTC_B_BCD) ? "no" : "yes",
           (status_b & RTC_B_24H) ? "yes" : "no");

    // Read current time from hardware.
    sec  = cmos_read(RTC_SECONDS);
    min  = cmos_read(RTC_MINUTES);
    hour = cmos_read(RTC_HOURS);
    day  = cmos_read(RTC_DAY);
    mon  = cmos_read(RTC_MONTH);
    year = cmos_read(RTC_YEAR);

    // Convert from BCD if needed.
    if (!(status_b & RTC_B_BCD)) {
        sec  = bcd2bin(sec);
        min  = bcd2bin(min);
        hour = bcd2bin(hour);
        day  = bcd2bin(day);
        mon  = bcd2bin(mon);
        year = bcd2bin(year);
    }

    // CMOS year is 0-99; assume 2000s.
    printk("<6>rtc-cmos: QEMU RTC time: 20%02d-%02d-%02d %02d:%02d:%02d UTC\n",
           year, mon, day, hour, min, sec);

    // Read seconds again to show the hardware clock is ticking.
    sec = cmos_read(RTC_SECONDS);
    if (!(status_b & RTC_B_BCD))
        sec = bcd2bin(sec);
    printk("<6>rtc-cmos: verification read: seconds = %d\n", sec);

    printk("<6>rtc-cmos: hardware RTC driver initialized successfully\n");
    return 0;
}

static void rtc_cmos_exit(void)
{
    printk("<6>rtc-cmos: driver unloading\n");
    if (rtc_region)
        __release_region((struct resource *)0, RTC_PORT_INDEX, RTC_PORT_LEN);
    printk("<6>rtc-cmos: done\n");
}

// Loader entry points.
int init_module(void) __attribute__((alias("rtc_cmos_init")));
void cleanup_module(void) __attribute__((alias("rtc_cmos_exit")));
