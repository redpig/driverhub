// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/symbols/symbol_registry.h"

#include <cstdint>
#include <cstdio>

#include "src/shim/kernel/bug.h"
#include "src/shim/kernel/device.h"
#include "src/shim/kernel/io.h"
#include "src/shim/kernel/irq.h"
#include "src/shim/kernel/memory.h"
#include "src/shim/kernel/module.h"
#include "src/shim/kernel/platform.h"
#include "src/shim/kernel/print.h"
#include "src/shim/kernel/sync.h"
#include "src/shim/kernel/time.h"
#include "src/shim/kernel/workqueue.h"
#include "src/shim/subsystem/cfg80211.h"
#include "src/shim/subsystem/drm.h"
#include "src/shim/subsystem/fs.h"
#include "src/shim/subsystem/gpio.h"
#include "src/shim/include/linux/gpio/driver.h"
#include "src/shim/subsystem/i2c.h"
#include "src/shim/subsystem/input.h"
#include "src/shim/subsystem/spi.h"
#include "src/shim/subsystem/usb.h"
#include "src/shim/subsystem/block.h"
#include "src/shim/subsystem/scsi.h"
#include "src/shim/kernel/clk.h"
#include "src/shim/subsystem/thermal.h"
#include "src/shim/subsystem/iio.h"
#include "src/shim/subsystem/pinctrl.h"
#include "src/shim/subsystem/watchdog.h"
#include "src/shim/subsystem/nvmem.h"
#include "src/shim/subsystem/power_supply.h"
#include "src/shim/subsystem/pwm.h"
#include "src/shim/kernel/libc_stubs.h"
#include "src/shim/subsystem/led.h"
#include "src/shim/kernel/gki_stubs.h"

// Forward declarations for RTC, timer, ABI-compat, and kunit shim symbols.
extern "C" {
// kunit
struct kunit;
void __kunit_do_failed_assertion(struct kunit* test,
                                 const void* assert_struct,
                                 int type,
                                 const char* file, int line,
                                 const char* fmt, ...);
void __kunit_abort(struct kunit* test);
void kunit_binary_assert_format(const void* assert_data,
                                const void* message,
                                char* buf, int buf_len);

// time64_to_tm
void time64_to_tm(long long totalsecs, int offset, void* result);

// rtc.h
struct rtc_device;
struct rtc_time;
struct rtc_wkalrm;
struct rtc_device *devm_rtc_allocate_device(struct device *parent);
int devm_rtc_register_device(struct rtc_device *rtc);
int __devm_rtc_register_device(void *owner, struct rtc_device *rtc);
void rtc_update_irq(struct rtc_device *rtc, unsigned long num,
                    unsigned long events);
void rtc_time64_to_tm(int64_t time, struct rtc_time *tm);
int64_t rtc_tm_to_time64(struct rtc_time *tm);
int64_t ktime_get_real_seconds(void);

// timer.h
struct timer_list;
void add_timer(struct timer_list *timer);
int del_timer(struct timer_list *timer);
int mod_timer(struct timer_list *timer, unsigned long expires);
void init_timer_key(struct timer_list *timer,
                    void (*func)(struct timer_list *),
                    unsigned int flags, const char *name, void *key);

// platform extras
struct platform_driver;
void platform_device_del(struct platform_device *pdev);
int __platform_driver_register(struct platform_driver *drv, void *owner);
int device_init_wakeup(struct device *dev, int enable);

// device model (rfkill)
void device_initialize(struct device* dev);
int device_add(struct device* dev);
void device_del(struct device* dev);
void put_device(struct device* dev);
int dev_set_name(struct device* dev, const char* fmt, ...);
struct dh_class;
int class_register(struct dh_class* cls);
void class_unregister(struct dh_class* cls);

// wait queue internals — already declared via sync.h include

// workqueue variants (rfkill)
int queue_work_on(int cpu, struct workqueue_struct* wq, struct work_struct* work);
int queue_delayed_work_on(int cpu, struct workqueue_struct* wq,
                          struct delayed_work* dwork, unsigned long delay);
void delayed_work_timer_fn(struct work_struct* work);
unsigned long round_jiffies_relative(unsigned long j);
extern struct workqueue_struct *system_wq;
extern struct workqueue_struct *system_power_efficient_wq;

// fs extras (rfkill)
int sysfs_emit(char *buf, const char *fmt, ...);
int kobject_uevent(struct kobject *kobj, int action);
struct kobj_uevent_env;
int add_uevent_var(struct kobj_uevent_env *env, const char *fmt, ...);
struct inode;
struct file;
int stream_open(struct inode *inode, struct file *filp);
long compat_ptr_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

// LED trigger register/unregister (rfkill)
struct led_trigger;
int led_trigger_register(struct led_trigger* trigger);
void led_trigger_unregister(struct led_trigger* trigger);

// Common kernel stubs for GKI modules
// loff_t already defined via fs.h
loff_t no_llseek(void* file, loff_t offset, int whence);
loff_t noop_llseek(void* file, loff_t offset, int whence);
int _cond_resched(void);
void __might_sleep(const char* file, int line);
void __might_fault(const char* file, int line);
int _printk(const char* fmt, ...);
int input_register_handler(void* handler);
void input_unregister_handler(void* handler);
void __warn_printk(const char* fmt, ...);
void _raw_spin_lock(void* lock);
void _raw_spin_unlock(void* lock);
void __sanitizer_cov_trace_pc(void);

// clk_hw (clock provider) stubs
struct clk_hw;
struct clk_hw* __clk_hw_register_gate(struct device* dev, const char* name,
                                       const char* parent_name,
                                       unsigned long flags,
                                       void* reg, uint8_t bit_idx,
                                       uint8_t clk_gate_flags, void* lock);
struct clk_hw* __clk_hw_register_fixed_rate(struct device* dev,
                                             const char* name,
                                             const char* parent_name,
                                             unsigned long flags,
                                             unsigned long fixed_rate);
const char* clk_hw_get_name(const struct clk_hw* hw);
unsigned long clk_hw_get_flags(const struct clk_hw* hw);
struct clk_hw* clk_hw_get_parent(struct clk_hw* hw);
unsigned long clk_hw_get_rate(const struct clk_hw* hw);
int clk_hw_is_enabled(const struct clk_hw* hw);
int clk_hw_is_prepared(const struct clk_hw* hw);
void clk_hw_unregister_gate(struct clk_hw* hw);
void clk_hw_unregister_fixed_rate(struct clk_hw* hw);

// platform_device_register_full
struct platform_device_info;
struct platform_device* platform_device_register_full(
    const struct platform_device_info* pdevinfo);

// input polling stubs
void input_setup_polling(struct input_dev* dev,
                          void (*poll)(struct input_dev*));
void input_set_poll_interval(struct input_dev* dev, unsigned int interval_ms);
unsigned int input_get_poll_interval(struct input_dev* dev);
void input_set_timestamp(struct input_dev* dev, long long timestamp);
long long input_get_timestamp(struct input_dev* dev);
int input_grab_device(void* handle);
void input_release_device(void* handle);
int input_match_device_id(const struct input_dev* dev, const void* id);
struct device* get_device(struct device* dev);

// kunit assert formatters
void kunit_unary_assert_format(const void* assert_data,
                                const void* message, char* buf, int buf_len);
void kunit_ptr_not_err_assert_format(const void* assert_data,
                                      const void* message, char* buf, int buf_len);
void kunit_fail_assert_format(const void* assert_data,
                               const void* message, char* buf, int buf_len);
void kunit_binary_str_assert_format(const void* assert_data,
                                     const void* message, char* buf, int buf_len);
void kunit_binary_ptr_assert_format(const void* assert_data,
                                     const void* message, char* buf, int buf_len);
void* kunit_kmalloc_array(struct kunit* test, unsigned long n,
                           unsigned long size, unsigned int gfp);

// rfkill stubs (misc kernel APIs)
void* dh_memcpy(void* dest, const void* src, unsigned long n);
int dh_strcmp(const char* s1, const char* s2);
unsigned long dh_strlen(const char* s);
int capable(int cap);
int kstrtoull(const char* s, unsigned int base, unsigned long long* res);
extern struct kernel_param_ops param_ops_uint;
unsigned long __arch_copy_from_user(void* to, const void* from, unsigned long n);
unsigned long __arch_copy_to_user(void* to, const void* from, unsigned long n);
void __check_object_size(const void* ptr, unsigned long n, int to_user);
int __list_add_valid_or_report(void* new_entry, void* prev, void* next);
int __list_del_entry_valid_or_report(void* entry);
void fortify_panic(const char* name);
void alt_cb_patch_nops(void* alt, int nr);
extern unsigned long* system_cpucaps;
unsigned long _raw_spin_lock_irqsave(void* lock);
void _raw_spin_unlock_irqrestore(void* lock, unsigned long flags);
extern void** kmalloc_caches;
void* kmalloc_trace(void* cache, unsigned int flags, unsigned long size);
void* __kmalloc(unsigned long size, unsigned int flags);
void __wake_up(void* wq_head, unsigned int mode, int nr_exclusive, void* key);
void __init_waitqueue_head(void* wq_head, const char* name, void* key);
void __mutex_init(void* mutex_ptr, const char* name, void* key);
}

namespace driverhub {

SymbolRegistry::SymbolRegistry() = default;
SymbolRegistry::~SymbolRegistry() = default;

void SymbolRegistry::Register(const std::string& name, void* address) {
  symbols_[name] = address;
}

void* SymbolRegistry::Resolve(std::string_view name) const {
  auto it = symbols_.find(std::string(name));
  if (it != symbols_.end()) {
    return it->second;
  }
  return nullptr;
}

bool SymbolRegistry::Contains(std::string_view name) const {
  return symbols_.find(std::string(name)) != symbols_.end();
}

// Macro to reduce boilerplate when registering shim symbols.
#define REGISTER_SYMBOL(sym) \
  Register(#sym, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&sym)))

void SymbolRegistry::RegisterKmiSymbols() {
  // Memory allocation
  REGISTER_SYMBOL(kmalloc);
  REGISTER_SYMBOL(kzalloc);
  REGISTER_SYMBOL(krealloc);
  REGISTER_SYMBOL(kcalloc);
  REGISTER_SYMBOL(kfree);
  REGISTER_SYMBOL(vmalloc);
  REGISTER_SYMBOL(vzalloc);
  REGISTER_SYMBOL(vfree);

  // Logging
  REGISTER_SYMBOL(printk);
  REGISTER_SYMBOL(dev_err);
  REGISTER_SYMBOL(dev_warn);
  REGISTER_SYMBOL(dev_info);
  REGISTER_SYMBOL(dev_dbg);

  // Synchronization — mutex
  REGISTER_SYMBOL(mutex_init);
  REGISTER_SYMBOL(mutex_destroy);
  REGISTER_SYMBOL(mutex_lock);
  REGISTER_SYMBOL(mutex_unlock);
  REGISTER_SYMBOL(mutex_trylock);

  // Synchronization — spinlock
  REGISTER_SYMBOL(spin_lock_init);
  REGISTER_SYMBOL(spin_lock);
  REGISTER_SYMBOL(spin_unlock);
  REGISTER_SYMBOL(spin_lock_irqsave);
  REGISTER_SYMBOL(spin_unlock_irqrestore);

  // Synchronization — wait queues
  REGISTER_SYMBOL(init_waitqueue_head);
  REGISTER_SYMBOL(wake_up);
  REGISTER_SYMBOL(wake_up_all);

  // Synchronization — completion
  REGISTER_SYMBOL(init_completion);
  REGISTER_SYMBOL(complete);
  REGISTER_SYMBOL(wait_for_completion);
  REGISTER_SYMBOL(wait_for_completion_timeout);

  // Module support
  REGISTER_SYMBOL(__driverhub_export_symbol);

  // Platform device
  REGISTER_SYMBOL(platform_driver_register);
  REGISTER_SYMBOL(platform_driver_unregister);
  REGISTER_SYMBOL(platform_get_resource);
  REGISTER_SYMBOL(platform_get_irq);
  REGISTER_SYMBOL(platform_device_alloc);
  REGISTER_SYMBOL(platform_device_add);
  REGISTER_SYMBOL(platform_device_put);
  REGISTER_SYMBOL(platform_device_unregister);
  REGISTER_SYMBOL(platform_device_register_full);

  // Device-managed allocation
  REGISTER_SYMBOL(devm_kmalloc);
  REGISTER_SYMBOL(devm_kzalloc);

  // Time
  REGISTER_SYMBOL(msecs_to_jiffies);
  REGISTER_SYMBOL(msleep);
  REGISTER_SYMBOL(usleep_range);
  REGISTER_SYMBOL(ktime_get);
  REGISTER_SYMBOL(ktime_get_real);

  // RTC subsystem
  REGISTER_SYMBOL(devm_rtc_allocate_device);
  REGISTER_SYMBOL(devm_rtc_register_device);
  REGISTER_SYMBOL(rtc_update_irq);
  REGISTER_SYMBOL(rtc_time64_to_tm);
  REGISTER_SYMBOL(rtc_tm_to_time64);
  REGISTER_SYMBOL(ktime_get_real_seconds);

  // Timer
  REGISTER_SYMBOL(add_timer);
  REGISTER_SYMBOL(del_timer);
  // timer_delete is a macro alias for del_timer in the module headers,
  // but register both names in case a module references either.
  Register("timer_delete", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&del_timer)));
  REGISTER_SYMBOL(mod_timer);

  // Platform extras (GKI .ko uses __ prefixed versions)
  REGISTER_SYMBOL(platform_device_del);
  REGISTER_SYMBOL(__platform_driver_register);
  REGISTER_SYMBOL(device_init_wakeup);

  // RTC — GKI .ko uses __devm_rtc_register_device (the non-underscore
  // version is a static inline wrapper in the kernel header).
  REGISTER_SYMBOL(__devm_rtc_register_device);

  // Timer — GKI .ko calls init_timer_key (invoked by timer_setup macro).
  REGISTER_SYMBOL(init_timer_key);

  // Jiffies (variable, not function)
  Register("jiffies", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(const_cast<volatile unsigned long*>(
          &jiffies))));

  // Workqueue
  REGISTER_SYMBOL(schedule_work);
  REGISTER_SYMBOL(cancel_work_sync);
  REGISTER_SYMBOL(flush_work);
  REGISTER_SYMBOL(create_singlethread_workqueue);
  REGISTER_SYMBOL(destroy_workqueue);
  REGISTER_SYMBOL(queue_work);
  REGISTER_SYMBOL(queue_delayed_work);
  REGISTER_SYMBOL(cancel_delayed_work_sync);
  REGISTER_SYMBOL(cancel_delayed_work);
  REGISTER_SYMBOL(flush_workqueue);

  // IRQ
  REGISTER_SYMBOL(request_irq);
  REGISTER_SYMBOL(request_threaded_irq);
  REGISTER_SYMBOL(free_irq);
  REGISTER_SYMBOL(disable_irq);
  REGISTER_SYMBOL(enable_irq);
  REGISTER_SYMBOL(disable_irq_nosync);
  REGISTER_SYMBOL(devm_request_irq);
  REGISTER_SYMBOL(devm_request_threaded_irq);

  // I/O mapping
  REGISTER_SYMBOL(ioremap);
  REGISTER_SYMBOL(iounmap);
  REGISTER_SYMBOL(ioremap_wc);
  REGISTER_SYMBOL(devm_ioremap);
  REGISTER_SYMBOL(devm_ioremap_resource);
  REGISTER_SYMBOL(ioread8);
  REGISTER_SYMBOL(ioread16);
  REGISTER_SYMBOL(ioread32);
  REGISTER_SYMBOL(iowrite8);
  REGISTER_SYMBOL(iowrite16);
  REGISTER_SYMBOL(iowrite32);

  // x86 port I/O
  REGISTER_SYMBOL(inb);
  REGISTER_SYMBOL(inw);
  REGISTER_SYMBOL(inl);
  REGISTER_SYMBOL(outb);
  REGISTER_SYMBOL(outw);
  REGISTER_SYMBOL(outl);
  REGISTER_SYMBOL(__request_region);
  REGISTER_SYMBOL(__release_region);

  // DMA
  REGISTER_SYMBOL(dma_alloc_coherent);
  REGISTER_SYMBOL(dma_free_coherent);
  REGISTER_SYMBOL(dma_set_mask);
  REGISTER_SYMBOL(dma_set_coherent_mask);
  REGISTER_SYMBOL(dma_set_mask_and_coherent);

  // User-space copy
  REGISTER_SYMBOL(copy_from_user);
  REGISTER_SYMBOL(copy_to_user);

  // BUG/WARN/panic
  REGISTER_SYMBOL(driverhub_bug);
  REGISTER_SYMBOL(driverhub_warn);
  REGISTER_SYMBOL(panic);
  // __warn_printk — used by GKI modules for WARN() output.
  Register("__warn_printk", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&printk)));

  // Stack protector
  REGISTER_SYMBOL(__stack_chk_fail);
  Register("__stack_chk_guard", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&__stack_chk_guard)));

  // KUnit test framework
  REGISTER_SYMBOL(__kunit_do_failed_assertion);
  REGISTER_SYMBOL(__kunit_abort);
  REGISTER_SYMBOL(kunit_binary_assert_format);
  REGISTER_SYMBOL(kunit_unary_assert_format);
  REGISTER_SYMBOL(kunit_ptr_not_err_assert_format);
  REGISTER_SYMBOL(kunit_fail_assert_format);
  REGISTER_SYMBOL(kunit_binary_str_assert_format);
  REGISTER_SYMBOL(kunit_binary_ptr_assert_format);
  REGISTER_SYMBOL(kunit_kmalloc_array);

  // Time — time64_to_tm (non-RTC variant, used by time_test.ko)
  REGISTER_SYMBOL(time64_to_tm);

  // I2C subsystem
  REGISTER_SYMBOL(i2c_add_driver);
  REGISTER_SYMBOL(i2c_del_driver);
  REGISTER_SYMBOL(i2c_transfer);
  REGISTER_SYMBOL(i2c_smbus_read_byte_data);
  REGISTER_SYMBOL(i2c_smbus_write_byte_data);
  REGISTER_SYMBOL(i2c_smbus_read_word_data);
  REGISTER_SYMBOL(i2c_smbus_write_word_data);
  REGISTER_SYMBOL(i2c_smbus_read_i2c_block_data);
  REGISTER_SYMBOL(i2c_new_client_device);
  REGISTER_SYMBOL(i2c_unregister_device);

  // SPI subsystem
  REGISTER_SYMBOL(spi_register_driver);
  REGISTER_SYMBOL(spi_unregister_driver);
  REGISTER_SYMBOL(spi_sync);
  REGISTER_SYMBOL(spi_async);
  REGISTER_SYMBOL(spi_write);
  REGISTER_SYMBOL(spi_read);
  REGISTER_SYMBOL(spi_write_then_read);

  // GPIO subsystem
  REGISTER_SYMBOL(gpio_request);
  REGISTER_SYMBOL(gpio_free);
  REGISTER_SYMBOL(gpio_direction_input);
  REGISTER_SYMBOL(gpio_direction_output);
  REGISTER_SYMBOL(gpio_get_value);
  REGISTER_SYMBOL(gpio_set_value);
  REGISTER_SYMBOL(gpio_to_irq);
  REGISTER_SYMBOL(gpio_is_valid);
  REGISTER_SYMBOL(devm_gpio_request);
  REGISTER_SYMBOL(devm_gpio_request_one);
  REGISTER_SYMBOL(gpiod_get);
  REGISTER_SYMBOL(gpiod_get_optional);
  REGISTER_SYMBOL(devm_gpiod_get);
  REGISTER_SYMBOL(devm_gpiod_get_optional);
  REGISTER_SYMBOL(gpiod_put);
  REGISTER_SYMBOL(gpiod_direction_input);
  REGISTER_SYMBOL(gpiod_direction_output);
  REGISTER_SYMBOL(gpiod_get_value);
  REGISTER_SYMBOL(gpiod_set_value);
  REGISTER_SYMBOL(gpiod_to_irq);
  // GPIO chip (controller/provider) API.
  REGISTER_SYMBOL(gpiochip_add_data);
  REGISTER_SYMBOL(gpiochip_remove);
  REGISTER_SYMBOL(devm_gpiochip_add_data);

  // USB subsystem
  REGISTER_SYMBOL(usb_register_driver);
  REGISTER_SYMBOL(usb_deregister);
  REGISTER_SYMBOL(usb_alloc_urb);
  REGISTER_SYMBOL(usb_free_urb);
  REGISTER_SYMBOL(usb_submit_urb);
  REGISTER_SYMBOL(usb_kill_urb);
  REGISTER_SYMBOL(usb_control_msg);
  REGISTER_SYMBOL(usb_bulk_msg);
  REGISTER_SYMBOL(usb_sndctrlpipe);
  REGISTER_SYMBOL(usb_rcvctrlpipe);
  REGISTER_SYMBOL(usb_sndbulkpipe);
  REGISTER_SYMBOL(usb_rcvbulkpipe);

  // DRM/KMS subsystem
  REGISTER_SYMBOL(drm_dev_alloc);
  REGISTER_SYMBOL(drm_dev_register);
  REGISTER_SYMBOL(drm_dev_unregister);
  REGISTER_SYMBOL(drm_dev_put);
  REGISTER_SYMBOL(drm_mode_config_init);
  REGISTER_SYMBOL(drm_mode_config_cleanup);
  REGISTER_SYMBOL(drm_connector_init);
  REGISTER_SYMBOL(drm_connector_cleanup);
  REGISTER_SYMBOL(drm_connector_register);
  REGISTER_SYMBOL(drm_connector_unregister);
  REGISTER_SYMBOL(drm_connector_helper_add);
  REGISTER_SYMBOL(drm_encoder_init);
  REGISTER_SYMBOL(drm_encoder_cleanup);
  REGISTER_SYMBOL(drm_crtc_init_with_planes);
  REGISTER_SYMBOL(drm_crtc_cleanup);
  REGISTER_SYMBOL(drm_crtc_helper_add);
  REGISTER_SYMBOL(drm_universal_plane_init);
  REGISTER_SYMBOL(drm_plane_cleanup);
  REGISTER_SYMBOL(drm_plane_helper_add);
  REGISTER_SYMBOL(drm_simple_display_pipe_init);
  REGISTER_SYMBOL(drm_framebuffer_init);
  REGISTER_SYMBOL(drm_framebuffer_cleanup);
  REGISTER_SYMBOL(drm_mode_duplicate);
  REGISTER_SYMBOL(drm_mode_probed_add);
  REGISTER_SYMBOL(drm_mode_set_name);
  REGISTER_SYMBOL(drm_atomic_helper_connector_reset);
  REGISTER_SYMBOL(drm_atomic_helper_connector_destroy_state);
  REGISTER_SYMBOL(drm_atomic_helper_connector_duplicate_state);
  REGISTER_SYMBOL(drm_gem_object_lookup);
  REGISTER_SYMBOL(drm_gem_object_put);
  REGISTER_SYMBOL(drm_panel_init);
  REGISTER_SYMBOL(drm_panel_add);
  REGISTER_SYMBOL(drm_panel_remove);
  REGISTER_SYMBOL(drm_panel_prepare);
  REGISTER_SYMBOL(drm_panel_unprepare);
  REGISTER_SYMBOL(drm_panel_enable);
  REGISTER_SYMBOL(drm_panel_disable);
  REGISTER_SYMBOL(devm_backlight_device_register);
  REGISTER_SYMBOL(backlight_enable);
  REGISTER_SYMBOL(backlight_disable);

  // Input subsystem (touch, buttons)
  REGISTER_SYMBOL(input_allocate_device);
  REGISTER_SYMBOL(devm_input_allocate_device);
  REGISTER_SYMBOL(input_free_device);
  REGISTER_SYMBOL(input_register_device);
  REGISTER_SYMBOL(input_unregister_device);
  REGISTER_SYMBOL(input_event);
  REGISTER_SYMBOL(input_report_key);
  REGISTER_SYMBOL(input_report_rel);
  REGISTER_SYMBOL(input_report_abs);
  REGISTER_SYMBOL(input_sync);
  REGISTER_SYMBOL(input_mt_sync);
  REGISTER_SYMBOL(input_mt_init_slots);
  REGISTER_SYMBOL(input_mt_slot);
  REGISTER_SYMBOL(input_mt_report_slot_state);
  REGISTER_SYMBOL(input_mt_sync_frame);
  REGISTER_SYMBOL(input_set_abs_params);
  REGISTER_SYMBOL(input_set_capability);

  // Input polling / utility
  REGISTER_SYMBOL(input_setup_polling);
  REGISTER_SYMBOL(input_set_poll_interval);
  REGISTER_SYMBOL(input_get_poll_interval);
  REGISTER_SYMBOL(input_set_timestamp);
  REGISTER_SYMBOL(input_get_timestamp);
  REGISTER_SYMBOL(input_grab_device);
  REGISTER_SYMBOL(input_release_device);
  REGISTER_SYMBOL(input_match_device_id);
  REGISTER_SYMBOL(get_device);

  // cfg80211/mac80211 WiFi subsystem
  REGISTER_SYMBOL(wiphy_new);
  REGISTER_SYMBOL(wiphy_new_nm);
  REGISTER_SYMBOL(wiphy_register);
  REGISTER_SYMBOL(wiphy_unregister);
  REGISTER_SYMBOL(wiphy_free);
  REGISTER_SYMBOL(ieee80211_alloc_hw);
  REGISTER_SYMBOL(ieee80211_register_hw);
  REGISTER_SYMBOL(ieee80211_unregister_hw);
  REGISTER_SYMBOL(ieee80211_free_hw);
  REGISTER_SYMBOL(cfg80211_scan_done);
  REGISTER_SYMBOL(cfg80211_inform_bss_data);
  REGISTER_SYMBOL(cfg80211_connect_result);
  REGISTER_SYMBOL(cfg80211_disconnected);
  REGISTER_SYMBOL(ieee80211_rx);
  REGISTER_SYMBOL(ieee80211_rx_irqsafe);
  REGISTER_SYMBOL(ieee80211_tx_status);
  REGISTER_SYMBOL(ieee80211_tx_status_irqsafe);
  REGISTER_SYMBOL(regulatory_hint);
  REGISTER_SYMBOL(alloc_netdev);
  REGISTER_SYMBOL(free_netdev);
  REGISTER_SYMBOL(register_netdev);
  REGISTER_SYMBOL(unregister_netdev);

  // VFS — character devices
  REGISTER_SYMBOL(cdev_init);
  REGISTER_SYMBOL(cdev_alloc);
  REGISTER_SYMBOL(cdev_add);
  REGISTER_SYMBOL(cdev_del);
  REGISTER_SYMBOL(register_chrdev_region);
  REGISTER_SYMBOL(alloc_chrdev_region);
  REGISTER_SYMBOL(unregister_chrdev_region);
  REGISTER_SYMBOL(register_chrdev);
  REGISTER_SYMBOL(unregister_chrdev);

  // VFS — class/device
  REGISTER_SYMBOL(class_create);
  REGISTER_SYMBOL(class_destroy);
  REGISTER_SYMBOL(device_create);
  REGISTER_SYMBOL(device_destroy);

  // VFS — misc device
  REGISTER_SYMBOL(misc_register);
  REGISTER_SYMBOL(misc_deregister);

  // VFS — seq_file
  REGISTER_SYMBOL(seq_printf);
  REGISTER_SYMBOL(seq_puts);
  REGISTER_SYMBOL(seq_putc);
  REGISTER_SYMBOL(seq_write);
  REGISTER_SYMBOL(seq_open);
  REGISTER_SYMBOL(seq_release);
  REGISTER_SYMBOL(seq_read);
  REGISTER_SYMBOL(seq_lseek);
  REGISTER_SYMBOL(single_open);
  REGISTER_SYMBOL(single_release);

  // VFS — procfs
  REGISTER_SYMBOL(proc_create);
  REGISTER_SYMBOL(proc_create_data);
  REGISTER_SYMBOL(proc_mkdir);
  REGISTER_SYMBOL(proc_remove);
  REGISTER_SYMBOL(remove_proc_entry);
  REGISTER_SYMBOL(proc_create_single_data);

  // VFS — debugfs
  REGISTER_SYMBOL(debugfs_create_dir);
  REGISTER_SYMBOL(debugfs_create_file);
  REGISTER_SYMBOL(debugfs_remove);
  REGISTER_SYMBOL(debugfs_remove_recursive);
  REGISTER_SYMBOL(debugfs_create_u8);
  REGISTER_SYMBOL(debugfs_create_u16);
  REGISTER_SYMBOL(debugfs_create_u32);
  REGISTER_SYMBOL(debugfs_create_u64);
  REGISTER_SYMBOL(debugfs_create_bool);
  REGISTER_SYMBOL(debugfs_create_blob);

  // VFS — sysfs
  REGISTER_SYMBOL(sysfs_create_group);
  REGISTER_SYMBOL(sysfs_remove_group);
  REGISTER_SYMBOL(sysfs_create_file);
  REGISTER_SYMBOL(sysfs_remove_file);
  REGISTER_SYMBOL(device_create_file);
  REGISTER_SYMBOL(device_remove_file);
  REGISTER_SYMBOL(sysfs_notify);

  // VFS — kobject
  REGISTER_SYMBOL(kobject_init);
  REGISTER_SYMBOL(kobject_add);
  REGISTER_SYMBOL(kobject_create_and_add);
  REGISTER_SYMBOL(kobject_put);
  REGISTER_SYMBOL(kobject_del);

  // Block layer — blk_mq
  REGISTER_SYMBOL(blk_mq_alloc_tag_set);
  REGISTER_SYMBOL(blk_mq_free_tag_set);
  REGISTER_SYMBOL(blk_mq_init_queue);
  REGISTER_SYMBOL(blk_mq_alloc_disk);
  REGISTER_SYMBOL(blk_mq_start_hw_queues);
  REGISTER_SYMBOL(blk_mq_stop_hw_queues);
  REGISTER_SYMBOL(blk_mq_start_stopped_hw_queues);
  REGISTER_SYMBOL(blk_mq_complete_request);
  REGISTER_SYMBOL(blk_mq_end_request);
  REGISTER_SYMBOL(blk_mq_start_request);
  REGISTER_SYMBOL(blk_mq_free_request);
  REGISTER_SYMBOL(blk_mq_kick_requeue_list);
  REGISTER_SYMBOL(blk_mq_requeue_request);
  REGISTER_SYMBOL(blk_mq_tagset_busy_iter);
  REGISTER_SYMBOL(blk_mq_map_queues);

  // Block layer — gendisk
  REGISTER_SYMBOL(alloc_disk);
  REGISTER_SYMBOL(add_disk);
  REGISTER_SYMBOL(del_gendisk);
  REGISTER_SYMBOL(put_disk);
  REGISTER_SYMBOL(set_capacity);
  REGISTER_SYMBOL(get_capacity);

  // Block layer — request queue config
  REGISTER_SYMBOL(blk_queue_logical_block_size);
  REGISTER_SYMBOL(blk_queue_physical_block_size);
  REGISTER_SYMBOL(blk_queue_max_hw_sectors);
  REGISTER_SYMBOL(blk_queue_max_segments);
  REGISTER_SYMBOL(blk_queue_max_segment_size);
  REGISTER_SYMBOL(blk_queue_dma_alignment);
  REGISTER_SYMBOL(blk_queue_flag_set);
  REGISTER_SYMBOL(blk_queue_flag_clear);
  REGISTER_SYMBOL(blk_queue_rq_timeout);

  // Block layer — bio
  REGISTER_SYMBOL(bio_alloc);
  REGISTER_SYMBOL(bio_put);
  REGISTER_SYMBOL(bio_endio);
  REGISTER_SYMBOL(submit_bio);

  // Block layer — registration
  REGISTER_SYMBOL(register_blkdev);
  REGISTER_SYMBOL(unregister_blkdev);

  // SCSI mid-layer
  REGISTER_SYMBOL(scsi_host_alloc);
  REGISTER_SYMBOL(scsi_add_host_with_dma);
  REGISTER_SYMBOL(scsi_scan_host);
  REGISTER_SYMBOL(scsi_remove_host);
  REGISTER_SYMBOL(scsi_host_put);
  REGISTER_SYMBOL(scsi_done);
  REGISTER_SYMBOL(scsi_change_queue_depth);
  REGISTER_SYMBOL(scsi_report_bus_reset);
  REGISTER_SYMBOL(scsi_dma_map);
  REGISTER_SYMBOL(scsi_dma_unmap);

  // DMA streaming mappings
  REGISTER_SYMBOL(dma_map_single);
  REGISTER_SYMBOL(dma_unmap_single);
  REGISTER_SYMBOL(dma_map_page);
  REGISTER_SYMBOL(dma_unmap_page);
  REGISTER_SYMBOL(dma_sync_single_for_cpu);
  REGISTER_SYMBOL(dma_sync_single_for_device);
  REGISTER_SYMBOL(dma_mapping_error);

  // Clock API
  REGISTER_SYMBOL(clk_get);
  REGISTER_SYMBOL(devm_clk_get);
  REGISTER_SYMBOL(devm_clk_get_optional);
  REGISTER_SYMBOL(clk_put);
  REGISTER_SYMBOL(clk_prepare);
  REGISTER_SYMBOL(clk_unprepare);
  REGISTER_SYMBOL(clk_enable);
  REGISTER_SYMBOL(clk_disable);
  REGISTER_SYMBOL(clk_prepare_enable);
  REGISTER_SYMBOL(clk_disable_unprepare);
  REGISTER_SYMBOL(clk_get_rate);
  REGISTER_SYMBOL(clk_set_rate);
  REGISTER_SYMBOL(clk_round_rate);

  // Clock provider (clk_hw) API
  REGISTER_SYMBOL(__clk_hw_register_gate);
  REGISTER_SYMBOL(__clk_hw_register_fixed_rate);
  REGISTER_SYMBOL(clk_hw_get_name);
  REGISTER_SYMBOL(clk_hw_get_flags);
  REGISTER_SYMBOL(clk_hw_get_parent);
  REGISTER_SYMBOL(clk_hw_get_rate);
  REGISTER_SYMBOL(clk_hw_is_enabled);
  REGISTER_SYMBOL(clk_hw_is_prepared);
  REGISTER_SYMBOL(clk_hw_unregister_gate);
  REGISTER_SYMBOL(clk_hw_unregister_fixed_rate);

  // Regulator API
  REGISTER_SYMBOL(regulator_get);
  REGISTER_SYMBOL(devm_regulator_get);
  REGISTER_SYMBOL(devm_regulator_get_optional);
  REGISTER_SYMBOL(regulator_put);
  REGISTER_SYMBOL(regulator_enable);
  REGISTER_SYMBOL(regulator_disable);
  REGISTER_SYMBOL(regulator_is_enabled);
  REGISTER_SYMBOL(regulator_set_voltage);
  REGISTER_SYMBOL(regulator_get_voltage);
  REGISTER_SYMBOL(regulator_set_load);

  // PM runtime
  REGISTER_SYMBOL(pm_runtime_enable);
  REGISTER_SYMBOL(pm_runtime_disable);
  REGISTER_SYMBOL(pm_runtime_get_sync);
  REGISTER_SYMBOL(pm_runtime_put);
  REGISTER_SYMBOL(pm_runtime_put_sync);
  REGISTER_SYMBOL(pm_runtime_set_active);
  REGISTER_SYMBOL(pm_runtime_set_autosuspend_delay);
  REGISTER_SYMBOL(pm_runtime_use_autosuspend);
  REGISTER_SYMBOL(pm_runtime_resume_and_get);
  REGISTER_SYMBOL(pm_runtime_get_noresume);
  REGISTER_SYMBOL(pm_runtime_put_noidle);
  REGISTER_SYMBOL(pm_runtime_suspended);

  // Thermal subsystem
  REGISTER_SYMBOL(thermal_zone_device_register);
  REGISTER_SYMBOL(thermal_zone_device_register_with_trips);
  REGISTER_SYMBOL(devm_thermal_of_zone_register);
  REGISTER_SYMBOL(thermal_zone_device_unregister);
  REGISTER_SYMBOL(thermal_zone_device_update);
  REGISTER_SYMBOL(thermal_zone_device_priv);
  REGISTER_SYMBOL(thermal_cooling_device_register);
  REGISTER_SYMBOL(devm_thermal_of_cooling_device_register);
  REGISTER_SYMBOL(thermal_cooling_device_unregister);
  REGISTER_SYMBOL(hwmon_device_register_with_info);
  REGISTER_SYMBOL(hwmon_device_unregister);
  REGISTER_SYMBOL(devm_hwmon_device_register_with_info);

  // IIO (sensors) subsystem
  REGISTER_SYMBOL(iio_device_alloc);
  REGISTER_SYMBOL(devm_iio_device_alloc);
  REGISTER_SYMBOL(iio_device_free);
  REGISTER_SYMBOL(iio_device_register);
  REGISTER_SYMBOL(devm_iio_device_register);
  REGISTER_SYMBOL(iio_device_unregister);
  REGISTER_SYMBOL(iio_priv);
  REGISTER_SYMBOL(iio_push_to_buffers_with_timestamp);
  REGISTER_SYMBOL(devm_iio_trigger_alloc);
  REGISTER_SYMBOL(devm_iio_trigger_register);
  REGISTER_SYMBOL(iio_trigger_notify_done);

  // Pin control subsystem
  REGISTER_SYMBOL(pinctrl_get);
  REGISTER_SYMBOL(devm_pinctrl_get);
  REGISTER_SYMBOL(pinctrl_put);
  REGISTER_SYMBOL(pinctrl_lookup_state);
  REGISTER_SYMBOL(pinctrl_select_state);
  REGISTER_SYMBOL(pinctrl_pm_select_default_state);
  REGISTER_SYMBOL(pinctrl_pm_select_sleep_state);
  REGISTER_SYMBOL(pinctrl_register);
  REGISTER_SYMBOL(devm_pinctrl_register);
  REGISTER_SYMBOL(pinctrl_unregister);
  REGISTER_SYMBOL(pinctrl_gpio_request);
  REGISTER_SYMBOL(pinctrl_gpio_free);
  REGISTER_SYMBOL(pinctrl_gpio_direction_input);
  REGISTER_SYMBOL(pinctrl_gpio_direction_output);

  // Watchdog subsystem
  REGISTER_SYMBOL(watchdog_init_timeout);
  REGISTER_SYMBOL(watchdog_register_device);
  REGISTER_SYMBOL(devm_watchdog_register_device);
  REGISTER_SYMBOL(watchdog_unregister_device);
  REGISTER_SYMBOL(watchdog_set_drvdata);
  REGISTER_SYMBOL(watchdog_get_drvdata);

  // NVMEM subsystem
  REGISTER_SYMBOL(nvmem_register);
  REGISTER_SYMBOL(devm_nvmem_register);
  REGISTER_SYMBOL(nvmem_unregister);
  REGISTER_SYMBOL(nvmem_cell_get);
  REGISTER_SYMBOL(devm_nvmem_cell_get);
  REGISTER_SYMBOL(nvmem_cell_put);
  REGISTER_SYMBOL(nvmem_cell_read);
  REGISTER_SYMBOL(nvmem_cell_write);
  REGISTER_SYMBOL(nvmem_device_read);
  REGISTER_SYMBOL(nvmem_device_write);

  // Power supply subsystem
  REGISTER_SYMBOL(power_supply_register);
  REGISTER_SYMBOL(devm_power_supply_register);
  REGISTER_SYMBOL(power_supply_unregister);
  REGISTER_SYMBOL(power_supply_changed);
  REGISTER_SYMBOL(power_supply_get_drvdata);
  REGISTER_SYMBOL(power_supply_set_drvdata);
  REGISTER_SYMBOL(power_supply_get_by_name);
  REGISTER_SYMBOL(power_supply_put);
  REGISTER_SYMBOL(power_supply_get_property);

  // PWM subsystem
  REGISTER_SYMBOL(pwm_request);
  REGISTER_SYMBOL(pwm_get);
  REGISTER_SYMBOL(devm_pwm_get);
  REGISTER_SYMBOL(pwm_free);
  REGISTER_SYMBOL(pwm_put);
  REGISTER_SYMBOL(pwm_config);
  REGISTER_SYMBOL(pwm_set_polarity);
  REGISTER_SYMBOL(pwm_enable);
  REGISTER_SYMBOL(pwm_disable);
  REGISTER_SYMBOL(pwm_apply_state);
  REGISTER_SYMBOL(pwm_get_state);
  REGISTER_SYMBOL(pwmchip_add);
  REGISTER_SYMBOL(pwmchip_remove);
  REGISTER_SYMBOL(devm_pwmchip_add);

  // LED subsystem
  REGISTER_SYMBOL(led_classdev_register);
  REGISTER_SYMBOL(devm_led_classdev_register);
  REGISTER_SYMBOL(devm_led_classdev_register_ext);
  REGISTER_SYMBOL(led_classdev_unregister);
  REGISTER_SYMBOL(led_set_brightness);
  REGISTER_SYMBOL(led_update_brightness);
  REGISTER_SYMBOL(led_blink_set);
  REGISTER_SYMBOL(led_blink_set_oneshot);
  REGISTER_SYMBOL(led_trigger_event);
  REGISTER_SYMBOL(led_trigger_blink);
  REGISTER_SYMBOL(led_trigger_register);
  REGISTER_SYMBOL(led_trigger_unregister);

  // Device model — lifecycle
  REGISTER_SYMBOL(device_initialize);
  REGISTER_SYMBOL(device_add);
  REGISTER_SYMBOL(device_del);
  REGISTER_SYMBOL(put_device);
  REGISTER_SYMBOL(dev_set_name);
  REGISTER_SYMBOL(class_register);
  REGISTER_SYMBOL(class_unregister);

  // Wait queue internals
  REGISTER_SYMBOL(init_wait_entry);
  REGISTER_SYMBOL(prepare_to_wait_event);
  REGISTER_SYMBOL(finish_wait);
  REGISTER_SYMBOL(schedule);

  // Workqueue — CPU-specific and timer variants
  REGISTER_SYMBOL(queue_work_on);
  REGISTER_SYMBOL(queue_delayed_work_on);
  REGISTER_SYMBOL(delayed_work_timer_fn);
  REGISTER_SYMBOL(round_jiffies_relative);
  // Global workqueue pointers (data symbols).
  Register("system_wq", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&system_wq)));
  Register("system_power_efficient_wq", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&system_power_efficient_wq)));

  // VFS extras
  REGISTER_SYMBOL(sysfs_emit);
  REGISTER_SYMBOL(kobject_uevent);
  REGISTER_SYMBOL(add_uevent_var);
  REGISTER_SYMBOL(stream_open);
  REGISTER_SYMBOL(compat_ptr_ioctl);

  // Kernel libc equivalents (mapped to shim wrappers)
  Register("memcpy", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&dh_memcpy)));
  Register("strcmp", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&dh_strcmp)));
  Register("strlen", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&dh_strlen)));

  // Misc kernel APIs
  REGISTER_SYMBOL(capable);
  REGISTER_SYMBOL(kstrtoull);
  Register("param_ops_uint", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&param_ops_uint)));
  REGISTER_SYMBOL(fortify_panic);

  // ARM64 arch stubs
  REGISTER_SYMBOL(__arch_copy_from_user);
  REGISTER_SYMBOL(__arch_copy_to_user);
  REGISTER_SYMBOL(__check_object_size);
  REGISTER_SYMBOL(alt_cb_patch_nops);
  Register("system_cpucaps", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&system_cpucaps)));

  // List validation (CONFIG_DEBUG_LIST)
  REGISTER_SYMBOL(__list_add_valid_or_report);
  REGISTER_SYMBOL(__list_del_entry_valid_or_report);

  // Raw spinlock variants
  REGISTER_SYMBOL(_raw_spin_lock_irqsave);
  REGISTER_SYMBOL(_raw_spin_unlock_irqrestore);

  // kmalloc internals (GKI 6.x)
  Register("kmalloc_caches", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&kmalloc_caches)));
  REGISTER_SYMBOL(kmalloc_trace);
  REGISTER_SYMBOL(__kmalloc);

  // Internal init functions
  REGISTER_SYMBOL(__wake_up);
  REGISTER_SYMBOL(__init_waitqueue_head);
  REGISTER_SYMBOL(__mutex_init);

  // Common kernel stubs
  REGISTER_SYMBOL(no_llseek);
  REGISTER_SYMBOL(noop_llseek);
  REGISTER_SYMBOL(_cond_resched);
  REGISTER_SYMBOL(__might_sleep);
  REGISTER_SYMBOL(__might_fault);
  REGISTER_SYMBOL(_printk);
  REGISTER_SYMBOL(input_register_handler);
  REGISTER_SYMBOL(input_unregister_handler);
  REGISTER_SYMBOL(__warn_printk);
  REGISTER_SYMBOL(_raw_spin_lock);
  REGISTER_SYMBOL(_raw_spin_unlock);
  REGISTER_SYMBOL(__sanitizer_cov_trace_pc);

  // ---- C library / fundamental kernel symbols ----
  // These are the most commonly needed symbols across GKI modules.
  // Analysis: 192 modules from GKI android15-6.6 system_dlkm.
  REGISTER_SYMBOL(memset);
  REGISTER_SYMBOL(memcpy);
  REGISTER_SYMBOL(memmove);
  REGISTER_SYMBOL(memcmp);
  REGISTER_SYMBOL(strlen);
  REGISTER_SYMBOL(strnlen);
  REGISTER_SYMBOL(strcmp);
  REGISTER_SYMBOL(strncmp);
  REGISTER_SYMBOL(strcpy);
  REGISTER_SYMBOL(strncpy);
  REGISTER_SYMBOL(strcat);
  REGISTER_SYMBOL(strncat);
  REGISTER_SYMBOL(strchr);
  REGISTER_SYMBOL(strrchr);
  REGISTER_SYMBOL(strstr);
  REGISTER_SYMBOL(snprintf);
  REGISTER_SYMBOL(sprintf);
  REGISTER_SYMBOL(sscanf);
  REGISTER_SYMBOL(vsnprintf);
  REGISTER_SYMBOL(vsprintf);
  REGISTER_SYMBOL(vsscanf);
  REGISTER_SYMBOL(strscpy);
  REGISTER_SYMBOL(kmemdup);

  // String conversion.
  REGISTER_SYMBOL(simple_strtol);
  REGISTER_SYMBOL(simple_strtoul);
  REGISTER_SYMBOL(simple_strtoll);
  REGISTER_SYMBOL(simple_strtoull);
  REGISTER_SYMBOL(kstrtoint);
  REGISTER_SYMBOL(kstrtouint);
  REGISTER_SYMBOL(kstrtol);
  REGISTER_SYMBOL(kstrtoul);
  REGISTER_SYMBOL(kstrtobool);

  // Refcount (needed by 29+ modules).
  REGISTER_SYMBOL(refcount_warn_saturate);

  // RCU (needed by 18+ modules).
  REGISTER_SYMBOL(__rcu_read_lock);
  REGISTER_SYMBOL(__rcu_read_unlock);
  REGISTER_SYMBOL(synchronize_rcu);
  REGISTER_SYMBOL(call_rcu);
  REGISTER_SYMBOL(rcu_barrier);

  // Preemption.
  REGISTER_SYMBOL(preempt_schedule_notrace);
  REGISTER_SYMBOL(preempt_schedule);

  // BH/read/write spinlock variants (needed by 15+ modules).
  REGISTER_SYMBOL(_raw_spin_lock_bh);
  REGISTER_SYMBOL(_raw_spin_unlock_bh);
  REGISTER_SYMBOL(_raw_read_lock);
  REGISTER_SYMBOL(_raw_read_unlock);
  REGISTER_SYMBOL(_raw_write_lock_bh);
  REGISTER_SYMBOL(_raw_write_unlock_bh);
  REGISTER_SYMBOL(_raw_read_lock_bh);
  REGISTER_SYMBOL(_raw_read_unlock_bh);

  // Module lifecycle.
  REGISTER_SYMBOL(try_module_get);
  REGISTER_SYMBOL(module_put);

  // Per-CPU / random.
  Register("cpu_number", reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&cpu_number)));
  REGISTER_SYMBOL(get_random_bytes);

  // dev_* logging.
  REGISTER_SYMBOL(_dev_err);
  REGISTER_SYMBOL(_dev_warn);
  REGISTER_SYMBOL(_dev_info);

  // Wait queue helpers.
  REGISTER_SYMBOL(add_wait_queue);
  REGISTER_SYMBOL(remove_wait_queue);

  // Sort/search.
  REGISTER_SYMBOL(sort);
  REGISTER_SYMBOL(bsearch);
  REGISTER_SYMBOL(full_name_hash);

  // ---- Auto-generated GKI module stubs (gki_stubs.cc) ----
  // Generated from GKI android15-6.6 kernel headers (build 14943902).
  // See tools/generate_stubs.py to regenerate.
  REGISTER_SYMBOL(__ClearPageMovable);
  REGISTER_SYMBOL(__SetPageMovable);
  REGISTER_SYMBOL(___pskb_trim);
  REGISTER_SYMBOL(___ratelimit);
  REGISTER_SYMBOL(__alloc_pages);
  REGISTER_SYMBOL(__alloc_percpu);
  REGISTER_SYMBOL(__alloc_percpu_gfp);
  REGISTER_SYMBOL(__alloc_skb);
  REGISTER_SYMBOL(__bio_add_page);
  REGISTER_SYMBOL(__bitmap_complement);
  REGISTER_SYMBOL(__blk_alloc_disk);
  REGISTER_SYMBOL(__blk_mq_alloc_disk);
  REGISTER_SYMBOL(__blk_rq_map_sg);
  REGISTER_SYMBOL(__clk_determine_rate);
  REGISTER_SYMBOL(__const_udelay);
  REGISTER_SYMBOL(__cpu_online_mask);
  REGISTER_SYMBOL(__cpu_possible_mask);
  REGISTER_SYMBOL(__cpuhp_remove_state);
  REGISTER_SYMBOL(__cpuhp_setup_state);
  REGISTER_SYMBOL(__crypto_memneq);
  REGISTER_SYMBOL(__dev_fwnode);
  REGISTER_SYMBOL(__dev_get_by_index);
  REGISTER_SYMBOL(__dev_get_by_name);
  REGISTER_SYMBOL(__dev_queue_xmit);
  REGISTER_SYMBOL(__devm_alloc_percpu);
  REGISTER_SYMBOL(__fdget);
  REGISTER_SYMBOL(__flush_workqueue);
  REGISTER_SYMBOL(__folio_put);
  REGISTER_SYMBOL(__free_pages);
  REGISTER_SYMBOL(__get_task_comm);
  REGISTER_SYMBOL(__hci_cmd_send);
  REGISTER_SYMBOL(__hci_cmd_sync);
  REGISTER_SYMBOL(__hci_cmd_sync_ev);
  REGISTER_SYMBOL(__hvc_resize);
  REGISTER_SYMBOL(__hw_addr_sync_dev);
  REGISTER_SYMBOL(__init_rwsem);
  REGISTER_SYMBOL(__ip_dev_find);
  REGISTER_SYMBOL(__ip_select_ident);
  REGISTER_SYMBOL(__ipv6_addr_type);
  REGISTER_SYMBOL(__kfifo_alloc);
  REGISTER_SYMBOL(__kfifo_free);
  REGISTER_SYMBOL(__kfifo_in);
  REGISTER_SYMBOL(__kfifo_out);
  REGISTER_SYMBOL(__kunit_add_resource);
  REGISTER_SYMBOL(__local_bh_enable_ip);
  REGISTER_SYMBOL(__mdiobus_register);
  REGISTER_SYMBOL(__module_get);
  REGISTER_SYMBOL(__msecs_to_jiffies);
  REGISTER_SYMBOL(__napi_alloc_skb);
  REGISTER_SYMBOL(__napi_schedule);
  REGISTER_SYMBOL(__netdev_alloc_skb);
  REGISTER_SYMBOL(__netif_napi_del);
  REGISTER_SYMBOL(__netif_rx);
  REGISTER_SYMBOL(__netlink_dump_start);
  REGISTER_SYMBOL(__nla_parse);
  REGISTER_SYMBOL(__nla_validate);
  REGISTER_SYMBOL(__nlmsg_put);
  REGISTER_SYMBOL(__num_online_cpus);
  REGISTER_SYMBOL(__pci_register_driver);
  REGISTER_SYMBOL(__per_cpu_offset);
  REGISTER_SYMBOL(__percpu_down_read);
  REGISTER_SYMBOL(__percpu_init_rwsem);
  REGISTER_SYMBOL(__pm_runtime_disable);
  REGISTER_SYMBOL(__pm_runtime_idle);
  REGISTER_SYMBOL(__pm_runtime_resume);
  REGISTER_SYMBOL(__pm_runtime_suspend);
  REGISTER_SYMBOL(__printk_ratelimit);
  REGISTER_SYMBOL(__pskb_copy_fclone);
  REGISTER_SYMBOL(__pskb_pull_tail);
  REGISTER_SYMBOL(__put_cred);
  REGISTER_SYMBOL(__put_net);
  REGISTER_SYMBOL(__put_task_struct);
  REGISTER_SYMBOL(__rb_erase_color);
  REGISTER_SYMBOL(__rb_insert_augmented);
  REGISTER_SYMBOL(__register_blkdev);
  REGISTER_SYMBOL(__register_chrdev);
  REGISTER_SYMBOL(__regmap_init);
  REGISTER_SYMBOL(__regmap_init_ram);
  REGISTER_SYMBOL(__regmap_init_raw_ram);
  REGISTER_SYMBOL(__request_module);
  REGISTER_SYMBOL(__rht_bucket_nested);
  REGISTER_SYMBOL(__sk_flush_backlog);
  REGISTER_SYMBOL(__sk_mem_reclaim);
  REGISTER_SYMBOL(__sk_receive_skb);
  REGISTER_SYMBOL(__skb_gso_segment);
  REGISTER_SYMBOL(__skb_pad);
  REGISTER_SYMBOL(__sock_create);
  REGISTER_SYMBOL(__sock_queue_rcv_skb);
  REGISTER_SYMBOL(__sock_recv_cmsgs);
  REGISTER_SYMBOL(__sock_recv_timestamp);
  REGISTER_SYMBOL(__sock_tx_timestamp);
  REGISTER_SYMBOL(__splice_from_pipe);
  REGISTER_SYMBOL(__srcu_read_lock);
  REGISTER_SYMBOL(__srcu_read_unlock);
  REGISTER_SYMBOL(__sysfs_match_string);
  REGISTER_SYMBOL(__tasklet_schedule);
  REGISTER_SYMBOL(__tty_alloc_driver);
  REGISTER_SYMBOL(__unregister_chrdev);
  REGISTER_SYMBOL(__usecs_to_jiffies);
  REGISTER_SYMBOL(__vmalloc);
  REGISTER_SYMBOL(__wake_up_sync_key);
  REGISTER_SYMBOL(_copy_from_iter);
  REGISTER_SYMBOL(_copy_to_iter);
  REGISTER_SYMBOL(_dev_notice);
  REGISTER_SYMBOL(_find_first_bit);
  REGISTER_SYMBOL(_find_first_zero_bit);
  REGISTER_SYMBOL(_find_next_bit);
  REGISTER_SYMBOL(_find_next_zero_bit);
  REGISTER_SYMBOL(_proc_mkdir);
  REGISTER_SYMBOL(_raw_spin_lock_irq);
  REGISTER_SYMBOL(_raw_spin_trylock);
  REGISTER_SYMBOL(_raw_spin_trylock_bh);
  REGISTER_SYMBOL(_raw_spin_unlock_irq);
  REGISTER_SYMBOL(_raw_write_lock);
  REGISTER_SYMBOL(_raw_write_unlock);
  REGISTER_SYMBOL(add_taint);
  REGISTER_SYMBOL(aes_encrypt);
  REGISTER_SYMBOL(aes_expandkey);
  REGISTER_SYMBOL(all_vm_events);
  REGISTER_SYMBOL(alloc_can_err_skb);
  REGISTER_SYMBOL(alloc_can_skb);
  REGISTER_SYMBOL(alloc_candev_mqs);
  REGISTER_SYMBOL(alloc_etherdev_mqs);
  REGISTER_SYMBOL(alloc_netdev_mqs);
  REGISTER_SYMBOL(alloc_pages_exact);
  REGISTER_SYMBOL(alloc_skb_with_frags);
  REGISTER_SYMBOL(alloc_workqueue);
  REGISTER_SYMBOL(anon_inode_getfd);
  REGISTER_SYMBOL(arc4_crypt);
  REGISTER_SYMBOL(arc4_setkey);
  REGISTER_SYMBOL(arm_smccc_1_2_hvc);
  REGISTER_SYMBOL(balloon_mops);
  REGISTER_SYMBOL(balloon_page_alloc);
  REGISTER_SYMBOL(balloon_page_dequeue);
  REGISTER_SYMBOL(balloon_page_enqueue);
  REGISTER_SYMBOL(baswap);
  REGISTER_SYMBOL(bcmp);
  REGISTER_SYMBOL(bin2hex);
  REGISTER_SYMBOL(bio_alloc_bioset);
  REGISTER_SYMBOL(bio_chain);
  REGISTER_SYMBOL(bio_init);
  REGISTER_SYMBOL(bio_start_io_acct);
  REGISTER_SYMBOL(bit_wait);
  REGISTER_SYMBOL(bit_wait_timeout);
  REGISTER_SYMBOL(blk_execute_rq);
  REGISTER_SYMBOL(blk_mq_alloc_request);
  REGISTER_SYMBOL(blk_mq_freeze_queue);
  REGISTER_SYMBOL(blk_mq_stop_hw_queue);
  REGISTER_SYMBOL(blk_mq_unfreeze_queue);
  REGISTER_SYMBOL(blk_queue_io_min);
  REGISTER_SYMBOL(blk_queue_io_opt);
  REGISTER_SYMBOL(blk_queue_write_cache);
  REGISTER_SYMBOL(blk_rq_map_kern);
  REGISTER_SYMBOL(blk_status_to_errno);
  REGISTER_SYMBOL(blkdev_get_by_dev);
  REGISTER_SYMBOL(blkdev_put);
  REGISTER_SYMBOL(bpf_trace_run1);
  REGISTER_SYMBOL(bpf_trace_run2);
  REGISTER_SYMBOL(bpf_trace_run3);
  REGISTER_SYMBOL(bpf_trace_run4);
  REGISTER_SYMBOL(bpf_trace_run5);
  REGISTER_SYMBOL(bpf_trace_run6);
  REGISTER_SYMBOL(bt_accept_dequeue);
  REGISTER_SYMBOL(bt_accept_enqueue);
  REGISTER_SYMBOL(bt_accept_unlink);
  REGISTER_SYMBOL(bt_debugfs);
  REGISTER_SYMBOL(bt_err);
  REGISTER_SYMBOL(bt_info);
  REGISTER_SYMBOL(bt_procfs_cleanup);
  REGISTER_SYMBOL(bt_procfs_init);
  REGISTER_SYMBOL(bt_sock_alloc);
  REGISTER_SYMBOL(bt_sock_ioctl);
  REGISTER_SYMBOL(bt_sock_link);
  REGISTER_SYMBOL(bt_sock_poll);
  REGISTER_SYMBOL(bt_sock_register);
  REGISTER_SYMBOL(bt_sock_unlink);
  REGISTER_SYMBOL(bt_sock_unregister);
  REGISTER_SYMBOL(bt_sock_wait_ready);
  REGISTER_SYMBOL(bt_sock_wait_state);
  REGISTER_SYMBOL(bt_warn);
  REGISTER_SYMBOL(btbcm_check_bdaddr);
  REGISTER_SYMBOL(btbcm_finalize);
  REGISTER_SYMBOL(btbcm_initialize);
  REGISTER_SYMBOL(btbcm_set_bdaddr);
  REGISTER_SYMBOL(bus_register);
  REGISTER_SYMBOL(bus_unregister);
  REGISTER_SYMBOL(can_bus_off);
  REGISTER_SYMBOL(can_change_mtu);
  REGISTER_SYMBOL(can_change_state);
  REGISTER_SYMBOL(can_proto_register);
  REGISTER_SYMBOL(can_proto_unregister);
  REGISTER_SYMBOL(can_rx_register);
  REGISTER_SYMBOL(can_rx_unregister);
  REGISTER_SYMBOL(can_send);
  REGISTER_SYMBOL(cancel_work);
  REGISTER_SYMBOL(cdc_parse_cdc_header);
  REGISTER_SYMBOL(check_zeroed_user);
  REGISTER_SYMBOL(class_dev_iter_exit);
  REGISTER_SYMBOL(class_dev_iter_init);
  REGISTER_SYMBOL(class_dev_iter_next);
  REGISTER_SYMBOL(class_find_device);
  REGISTER_SYMBOL(class_for_each_device);
  REGISTER_SYMBOL(cleanup_srcu_struct);
  REGISTER_SYMBOL(clk_get_parent);
  REGISTER_SYMBOL(clk_has_parent);
  REGISTER_SYMBOL(clk_hw_get_clk);
  REGISTER_SYMBOL(clk_hw_register);
  REGISTER_SYMBOL(clk_hw_unregister);
  REGISTER_SYMBOL(clk_is_match);
  REGISTER_SYMBOL(clk_notifier_register);
  REGISTER_SYMBOL(clk_set_parent);
  REGISTER_SYMBOL(clk_set_rate_range);
  REGISTER_SYMBOL(close_candev);
  REGISTER_SYMBOL(consume_skb);
  REGISTER_SYMBOL(crc16);
  REGISTER_SYMBOL(crc32_le);
  REGISTER_SYMBOL(crc_ccitt);
  REGISTER_SYMBOL(crypto_aead_decrypt);
  REGISTER_SYMBOL(crypto_aead_encrypt);
  REGISTER_SYMBOL(crypto_aead_setkey);
  REGISTER_SYMBOL(crypto_alloc_aead);
  REGISTER_SYMBOL(crypto_alloc_base);
  REGISTER_SYMBOL(crypto_alloc_kpp);
  REGISTER_SYMBOL(crypto_alloc_shash);
  REGISTER_SYMBOL(crypto_comp_compress);
  REGISTER_SYMBOL(crypto_default_rng);
  REGISTER_SYMBOL(crypto_destroy_tfm);
  REGISTER_SYMBOL(crypto_ecdh_key_len);
  REGISTER_SYMBOL(crypto_has_ahash);
  REGISTER_SYMBOL(crypto_has_alg);
  REGISTER_SYMBOL(crypto_req_done);
  REGISTER_SYMBOL(crypto_shash_digest);
  REGISTER_SYMBOL(crypto_shash_final);
  REGISTER_SYMBOL(crypto_shash_setkey);
  REGISTER_SYMBOL(crypto_shash_update);
  REGISTER_SYMBOL(csum_ipv6_magic);
  REGISTER_SYMBOL(current_work);
  REGISTER_SYMBOL(datagram_poll);
  REGISTER_SYMBOL(debugfs_attr_read);
  REGISTER_SYMBOL(debugfs_attr_write);
  REGISTER_SYMBOL(debugfs_initialized);
  REGISTER_SYMBOL(debugfs_lookup);
  REGISTER_SYMBOL(dec_zone_page_state);
  REGISTER_SYMBOL(default_llseek);
  REGISTER_SYMBOL(default_wake_function);
  REGISTER_SYMBOL(dev_add_pack);
  REGISTER_SYMBOL(dev_addr_add);
  REGISTER_SYMBOL(dev_addr_del);
  REGISTER_SYMBOL(dev_addr_mod);
  REGISTER_SYMBOL(dev_alloc_name);
  REGISTER_SYMBOL(dev_change_flags);
  REGISTER_SYMBOL(dev_close_many);
  REGISTER_SYMBOL(dev_coredumpv);
  REGISTER_SYMBOL(dev_err_probe);
  REGISTER_SYMBOL(dev_fetch_sw_netstats);
  REGISTER_SYMBOL(dev_get_by_index);
  REGISTER_SYMBOL(dev_get_by_index_rcu);
  REGISTER_SYMBOL(dev_get_by_name);
  REGISTER_SYMBOL(dev_get_flags);
  REGISTER_SYMBOL(dev_get_stats);
  REGISTER_SYMBOL(dev_get_tstats64);
  REGISTER_SYMBOL(dev_getbyhwaddr_rcu);
  REGISTER_SYMBOL(dev_getfirstbyhwtype);
  REGISTER_SYMBOL(dev_load);
  REGISTER_SYMBOL(dev_mc_sync);
  REGISTER_SYMBOL(dev_mc_unsync);
  REGISTER_SYMBOL(dev_nit_active);
  REGISTER_SYMBOL(dev_remove_pack);
  REGISTER_SYMBOL(dev_set_allmulti);
  REGISTER_SYMBOL(dev_set_mac_address);
  REGISTER_SYMBOL(dev_set_mtu);
  REGISTER_SYMBOL(dev_set_promiscuity);
  REGISTER_SYMBOL(dev_uc_add);
  REGISTER_SYMBOL(dev_uc_add_excl);
  REGISTER_SYMBOL(dev_uc_del);
  REGISTER_SYMBOL(dev_uc_sync);
  REGISTER_SYMBOL(dev_uc_unsync);
  REGISTER_SYMBOL(device_add_disk);
  REGISTER_SYMBOL(device_find_any_child);
  REGISTER_SYMBOL(device_for_each_child);
  REGISTER_SYMBOL(device_get_match_data);
  REGISTER_SYMBOL(device_match_name);
  REGISTER_SYMBOL(device_move);
  REGISTER_SYMBOL(device_register);
  REGISTER_SYMBOL(device_rename);
  REGISTER_SYMBOL(device_unregister);
  REGISTER_SYMBOL(device_wakeup_disable);
  REGISTER_SYMBOL(device_wakeup_enable);
  REGISTER_SYMBOL(devm_clk_put);
  REGISTER_SYMBOL(devm_free_irq);
  REGISTER_SYMBOL(devm_hwrng_register);
  REGISTER_SYMBOL(devm_kfree);
  REGISTER_SYMBOL(devm_kstrdup);
  REGISTER_SYMBOL(devm_memremap);
  REGISTER_SYMBOL(devm_memunmap);
  REGISTER_SYMBOL(disk_set_zoned);
  REGISTER_SYMBOL(dma_alloc_attrs);
  REGISTER_SYMBOL(dma_free_attrs);
  REGISTER_SYMBOL(down_read);
  REGISTER_SYMBOL(down_write);
  REGISTER_SYMBOL(dput);
  REGISTER_SYMBOL(drain_workqueue);
  REGISTER_SYMBOL(driver_attach);
  REGISTER_SYMBOL(driver_register);
  REGISTER_SYMBOL(driver_unregister);
  REGISTER_SYMBOL(dst_cache_destroy);
  REGISTER_SYMBOL(dst_cache_get);
  REGISTER_SYMBOL(dst_cache_init);
  REGISTER_SYMBOL(dst_cache_set_ip4);
  REGISTER_SYMBOL(dst_cache_set_ip6);
  REGISTER_SYMBOL(dst_release);
  REGISTER_SYMBOL(efi);
  REGISTER_SYMBOL(eth_header_parse);
  REGISTER_SYMBOL(eth_mac_addr);
  REGISTER_SYMBOL(eth_type_trans);
  REGISTER_SYMBOL(eth_validate_addr);
  REGISTER_SYMBOL(ether_setup);
  REGISTER_SYMBOL(ethtool_op_get_link);
  REGISTER_SYMBOL(eventfd_ctx_do_read);
  REGISTER_SYMBOL(eventfd_ctx_fdget);
  REGISTER_SYMBOL(eventfd_ctx_fileget);
  REGISTER_SYMBOL(eventfd_ctx_put);
  REGISTER_SYMBOL(eventfd_signal);
  REGISTER_SYMBOL(fasync_helper);
  REGISTER_SYMBOL(fat_time_fat2unix);
  REGISTER_SYMBOL(fat_time_unix2fat);
  REGISTER_SYMBOL(fget);
  REGISTER_SYMBOL(file_path);
  REGISTER_SYMBOL(filp_close);
  REGISTER_SYMBOL(filp_open_block);
  REGISTER_SYMBOL(finish_rcuwait);
  REGISTER_SYMBOL(flush_dcache_page);
  REGISTER_SYMBOL(flush_delayed_work);
  REGISTER_SYMBOL(folio_wait_bit);
  REGISTER_SYMBOL(fput);
  REGISTER_SYMBOL(fqdir_exit);
  REGISTER_SYMBOL(fqdir_init);
  REGISTER_SYMBOL(free_candev);
  REGISTER_SYMBOL(free_pages);
  REGISTER_SYMBOL(free_pages_exact);
  REGISTER_SYMBOL(free_percpu);
  REGISTER_SYMBOL(fs_bio_set);
  REGISTER_SYMBOL(generic_mii_ioctl);
  REGISTER_SYMBOL(genl_register_family);
  REGISTER_SYMBOL(genlmsg_put);
  REGISTER_SYMBOL(genphy_resume);
  REGISTER_SYMBOL(get_net_ns_by_fd);
  REGISTER_SYMBOL(get_net_ns_by_pid);
  REGISTER_SYMBOL(get_random_u32);
  REGISTER_SYMBOL(get_user_ifreq);
  REGISTER_SYMBOL(get_zeroed_page);
  REGISTER_SYMBOL(glob_match);
  REGISTER_SYMBOL(gpiochip_get_data);
  REGISTER_SYMBOL(gre_add_protocol);
  REGISTER_SYMBOL(gre_del_protocol);
  REGISTER_SYMBOL(gro_cells_destroy);
  REGISTER_SYMBOL(gro_cells_init);
  REGISTER_SYMBOL(gro_cells_receive);
  REGISTER_SYMBOL(hci_alloc_dev_priv);
  REGISTER_SYMBOL(hci_conn_check_secure);
  REGISTER_SYMBOL(hci_conn_security);
  REGISTER_SYMBOL(hci_conn_switch_role);
  REGISTER_SYMBOL(hci_devcd_abort);
  REGISTER_SYMBOL(hci_devcd_append);
  REGISTER_SYMBOL(hci_devcd_complete);
  REGISTER_SYMBOL(hci_devcd_init);
  REGISTER_SYMBOL(hci_devcd_register);
  REGISTER_SYMBOL(hci_free_dev);
  REGISTER_SYMBOL(hci_get_route);
  REGISTER_SYMBOL(hci_recv_diag);
  REGISTER_SYMBOL(hci_recv_frame);
  REGISTER_SYMBOL(hci_register_cb);
  REGISTER_SYMBOL(hci_register_dev);
  REGISTER_SYMBOL(hci_reset_dev);
  REGISTER_SYMBOL(hci_set_fw_info);
  REGISTER_SYMBOL(hci_unregister_cb);
  REGISTER_SYMBOL(hci_unregister_dev);
  REGISTER_SYMBOL(hex2bin);
  REGISTER_SYMBOL(hex_asc_upper);
  REGISTER_SYMBOL(hex_to_bin);
  REGISTER_SYMBOL(hid_add_device);
  REGISTER_SYMBOL(hid_allocate_device);
  REGISTER_SYMBOL(hid_destroy_device);
  REGISTER_SYMBOL(hid_ignore);
  REGISTER_SYMBOL(hid_input_report);
  REGISTER_SYMBOL(hid_is_usb);
  REGISTER_SYMBOL(hid_parse_report);
  REGISTER_SYMBOL(hrtimer_active);
  REGISTER_SYMBOL(hrtimer_cancel);
  REGISTER_SYMBOL(hrtimer_forward);
  REGISTER_SYMBOL(hrtimer_init);
  REGISTER_SYMBOL(hvc_alloc);
  REGISTER_SYMBOL(hvc_instantiate);
  REGISTER_SYMBOL(hvc_kick);
  REGISTER_SYMBOL(hvc_poll);
  REGISTER_SYMBOL(hvc_remove);
  REGISTER_SYMBOL(ida_alloc_range);
  REGISTER_SYMBOL(ida_destroy);
  REGISTER_SYMBOL(ida_free);
  REGISTER_SYMBOL(idr_alloc);
  REGISTER_SYMBOL(idr_alloc_u32);
  REGISTER_SYMBOL(idr_destroy);
  REGISTER_SYMBOL(idr_find);
  REGISTER_SYMBOL(idr_for_each);
  REGISTER_SYMBOL(idr_get_next);
  REGISTER_SYMBOL(idr_get_next_ul);
  REGISTER_SYMBOL(idr_preload);
  REGISTER_SYMBOL(idr_remove);
  REGISTER_SYMBOL(idr_replace);
  REGISTER_SYMBOL(ieee802154_hdr_peek);
  REGISTER_SYMBOL(ieee802154_hdr_pull);
  REGISTER_SYMBOL(ieee802154_hdr_push);
  REGISTER_SYMBOL(iio_format_value);
  REGISTER_SYMBOL(in6addr_any);
  REGISTER_SYMBOL(in_aton);
  REGISTER_SYMBOL(inc_zone_page_state);
  REGISTER_SYMBOL(inet6_csk_xmit);
  REGISTER_SYMBOL(inet_frag_destroy);
  REGISTER_SYMBOL(inet_frag_find);
  REGISTER_SYMBOL(inet_frag_kill);
  REGISTER_SYMBOL(inet_frags_fini);
  REGISTER_SYMBOL(inet_frags_init);
  REGISTER_SYMBOL(init_net);
  REGISTER_SYMBOL(init_on_free);
  REGISTER_SYMBOL(init_srcu_struct);
  REGISTER_SYMBOL(init_user_ns);
  REGISTER_SYMBOL(init_uts_ns);
  REGISTER_SYMBOL(iov_iter_bvec);
  REGISTER_SYMBOL(iov_iter_get_pages2);
  REGISTER_SYMBOL(iov_iter_init);
  REGISTER_SYMBOL(iov_iter_kvec);
  REGISTER_SYMBOL(iov_iter_npages);
  REGISTER_SYMBOL(iov_iter_revert);
  REGISTER_SYMBOL(ip6_dst_hoplimit);
  REGISTER_SYMBOL(ip_local_out);
  REGISTER_SYMBOL(ip_mc_join_group);
  REGISTER_SYMBOL(ip_queue_xmit);
  REGISTER_SYMBOL(ip_route_output_flow);
  REGISTER_SYMBOL(ip_send_check);
  REGISTER_SYMBOL(ipv6_dev_find);
  REGISTER_SYMBOL(ipv6_stub);
  REGISTER_SYMBOL(irq_get_irq_data);
  REGISTER_SYMBOL(irq_set_irq_wake);
  REGISTER_SYMBOL(jiffies_to_msecs);
  REGISTER_SYMBOL(jiffies_to_usecs);
  REGISTER_SYMBOL(kasan_flag_enabled);
  REGISTER_SYMBOL(kasprintf);
  REGISTER_SYMBOL(kernel_accept);
  REGISTER_SYMBOL(kernel_bind);
  REGISTER_SYMBOL(kernel_connect);
  REGISTER_SYMBOL(kernel_kobj);
  REGISTER_SYMBOL(kernel_listen);
  REGISTER_SYMBOL(kernel_read);
  REGISTER_SYMBOL(kernel_sendmsg);
  REGISTER_SYMBOL(kernel_sock_shutdown);
  REGISTER_SYMBOL(kernel_write);
  REGISTER_SYMBOL(kfree_const);
  REGISTER_SYMBOL(kfree_sensitive);
  REGISTER_SYMBOL(kfree_skb_list_reason);
  REGISTER_SYMBOL(kfree_skb_partial);
  REGISTER_SYMBOL(kfree_skb_reason);
  REGISTER_SYMBOL(kill_fasync);
  REGISTER_SYMBOL(kimage_voffset);
  REGISTER_SYMBOL(kmalloc_large);
  REGISTER_SYMBOL(kmalloc_large_node);
  REGISTER_SYMBOL(kmalloc_node_trace);
  REGISTER_SYMBOL(kmem_cache_alloc);
  REGISTER_SYMBOL(kmem_cache_create);
  REGISTER_SYMBOL(kmem_cache_destroy);
  REGISTER_SYMBOL(kmem_cache_free);
  REGISTER_SYMBOL(kobject_get);
  REGISTER_SYMBOL(kstrdup);
  REGISTER_SYMBOL(kstrndup);
  REGISTER_SYMBOL(kstrtobool_from_user);
  REGISTER_SYMBOL(kstrtoll);
  REGISTER_SYMBOL(kstrtou16);
  REGISTER_SYMBOL(kstrtou8);
  REGISTER_SYMBOL(kthread_create_worker);
  REGISTER_SYMBOL(kthread_should_stop);
  REGISTER_SYMBOL(kthread_stop);
  REGISTER_SYMBOL(ktime_get_real_ts64);
  REGISTER_SYMBOL(ktime_get_snapshot);
  REGISTER_SYMBOL(ktime_get_ts64);
  REGISTER_SYMBOL(ktime_get_with_offset);
  REGISTER_SYMBOL(kunit_add_action);
  REGISTER_SYMBOL(kunit_cleanup);
  REGISTER_SYMBOL(kunit_hooks);
  REGISTER_SYMBOL(kunit_init_test);
  REGISTER_SYMBOL(kunit_log_append);
  REGISTER_SYMBOL(kunit_release_action);
  REGISTER_SYMBOL(kunit_remove_action);
  REGISTER_SYMBOL(kunit_remove_resource);
  REGISTER_SYMBOL(kunit_running);
  REGISTER_SYMBOL(kunit_try_catch_run);
  REGISTER_SYMBOL(kunit_try_catch_throw);
  REGISTER_SYMBOL(kvasprintf_const);
  REGISTER_SYMBOL(kvfree);
  REGISTER_SYMBOL(kvfree_call_rcu);
  REGISTER_SYMBOL(kvmalloc_node);
  REGISTER_SYMBOL(l2cap_conn_get);
  REGISTER_SYMBOL(l2cap_conn_put);
  REGISTER_SYMBOL(l2cap_is_socket);
  REGISTER_SYMBOL(l2cap_register_user);
  REGISTER_SYMBOL(l2cap_unregister_user);
  REGISTER_SYMBOL(l2tp_session_create);
  REGISTER_SYMBOL(l2tp_session_delete);
  REGISTER_SYMBOL(l2tp_session_get_nth);
  REGISTER_SYMBOL(l2tp_session_register);
  REGISTER_SYMBOL(l2tp_tunnel_create);
  REGISTER_SYMBOL(l2tp_tunnel_delete);
  REGISTER_SYMBOL(l2tp_tunnel_get);
  REGISTER_SYMBOL(l2tp_tunnel_get_nth);
  REGISTER_SYMBOL(l2tp_tunnel_register);
  REGISTER_SYMBOL(l2tp_udp_encap_recv);
  REGISTER_SYMBOL(l2tp_xmit_skb);
  REGISTER_SYMBOL(linkwatch_fire_event);
  REGISTER_SYMBOL(list_sort);
  REGISTER_SYMBOL(lock_sock_nested);
  REGISTER_SYMBOL(log_post_read_mmio);
  REGISTER_SYMBOL(log_post_write_mmio);
  REGISTER_SYMBOL(log_read_mmio);
  REGISTER_SYMBOL(log_write_mmio);
  REGISTER_SYMBOL(lowpan_nhc_add);
  REGISTER_SYMBOL(lowpan_nhc_del);
  REGISTER_SYMBOL(match_int);
  REGISTER_SYMBOL(match_strdup);
  REGISTER_SYMBOL(match_token);
  REGISTER_SYMBOL(mdiobus_alloc_size);
  REGISTER_SYMBOL(mdiobus_free);
  REGISTER_SYMBOL(mdiobus_get_phy);
  REGISTER_SYMBOL(mdiobus_unregister);
  REGISTER_SYMBOL(memchr);
  REGISTER_SYMBOL(memchr_inv);
  REGISTER_SYMBOL(memcpy_and_pad);
  REGISTER_SYMBOL(memdup_user);
  REGISTER_SYMBOL(memparse);
  REGISTER_SYMBOL(memscan);
  REGISTER_SYMBOL(memset64);
  REGISTER_SYMBOL(memstart_addr);
  REGISTER_SYMBOL(metadata_dst_alloc);
  REGISTER_SYMBOL(mii_check_media);
  REGISTER_SYMBOL(mii_ethtool_gset);
  REGISTER_SYMBOL(mii_link_ok);
  REGISTER_SYMBOL(mii_nway_restart);
  REGISTER_SYMBOL(mtree_load);
  REGISTER_SYMBOL(mutex_is_locked);
  REGISTER_SYMBOL(n_tty_ioctl_helper);
  REGISTER_SYMBOL(napi_complete_done);
  REGISTER_SYMBOL(napi_disable);
  REGISTER_SYMBOL(napi_enable);
  REGISTER_SYMBOL(napi_gro_receive);
  REGISTER_SYMBOL(napi_schedule_prep);
  REGISTER_SYMBOL(nd_tbl);
  REGISTER_SYMBOL(neigh_destroy);
  REGISTER_SYMBOL(neigh_lookup);
  REGISTER_SYMBOL(net_namespace_list);
  REGISTER_SYMBOL(net_ratelimit);
  REGISTER_SYMBOL(net_selftest);
  REGISTER_SYMBOL(netdev_err);
  REGISTER_SYMBOL(netdev_info);
  REGISTER_SYMBOL(netdev_name_in_use);
  REGISTER_SYMBOL(netdev_notice);
  REGISTER_SYMBOL(netdev_printk);
  REGISTER_SYMBOL(netdev_upper_dev_link);
  REGISTER_SYMBOL(netdev_warn);
  REGISTER_SYMBOL(netif_carrier_off);
  REGISTER_SYMBOL(netif_carrier_on);
  REGISTER_SYMBOL(netif_device_attach);
  REGISTER_SYMBOL(netif_device_detach);
  REGISTER_SYMBOL(netif_inherit_tso_max);
  REGISTER_SYMBOL(netif_napi_add_weight);
  REGISTER_SYMBOL(netif_receive_skb);
  REGISTER_SYMBOL(netif_rx);
  REGISTER_SYMBOL(netif_tx_lock);
  REGISTER_SYMBOL(netif_tx_unlock);
  REGISTER_SYMBOL(netif_tx_wake_queue);
  REGISTER_SYMBOL(netlink_broadcast);
  REGISTER_SYMBOL(netlink_capable);
  REGISTER_SYMBOL(netlink_net_capable);
  REGISTER_SYMBOL(netlink_unicast);
  REGISTER_SYMBOL(next_arg);
  REGISTER_SYMBOL(nf_conntrack_destroy);
  REGISTER_SYMBOL(nl802154_scan_done);
  REGISTER_SYMBOL(nl802154_scan_event);
  REGISTER_SYMBOL(nl802154_scan_started);
  REGISTER_SYMBOL(nla_memcpy);
  REGISTER_SYMBOL(nla_put);
  REGISTER_SYMBOL(nla_put_64bit);
  REGISTER_SYMBOL(nla_strscpy);
  REGISTER_SYMBOL(nonseekable_open);
  REGISTER_SYMBOL(nr_cpu_ids);
  REGISTER_SYMBOL(ns_capable);
  REGISTER_SYMBOL(ns_to_timespec64);
  REGISTER_SYMBOL(of_get_child_by_name);
  REGISTER_SYMBOL(of_irq_get_byname);
  REGISTER_SYMBOL(open_candev);
  REGISTER_SYMBOL(overflowgid);
  REGISTER_SYMBOL(overflowuid);
  REGISTER_SYMBOL(p9_client_cb);
  REGISTER_SYMBOL(p9_parse_header);
  REGISTER_SYMBOL(p9_req_put);
  REGISTER_SYMBOL(p9_tag_lookup);
  REGISTER_SYMBOL(page_pinner_inited);
  REGISTER_SYMBOL(page_relinquish);
  REGISTER_SYMBOL(param_ops_bool);
  REGISTER_SYMBOL(param_ops_charp);
  REGISTER_SYMBOL(param_ops_int);
  REGISTER_SYMBOL(pci_device_is_present);
  REGISTER_SYMBOL(pci_disable_device);
  REGISTER_SYMBOL(pci_disable_sriov);
  REGISTER_SYMBOL(pci_enable_device);
  REGISTER_SYMBOL(pci_enable_sriov);
  REGISTER_SYMBOL(pci_find_capability);
  REGISTER_SYMBOL(pci_free_irq_vectors);
  REGISTER_SYMBOL(pci_iomap);
  REGISTER_SYMBOL(pci_iomap_range);
  REGISTER_SYMBOL(pci_iounmap);
  REGISTER_SYMBOL(pci_irq_get_affinity);
  REGISTER_SYMBOL(pci_irq_vector);
  REGISTER_SYMBOL(pci_read_config_byte);
  REGISTER_SYMBOL(pci_read_config_dword);
  REGISTER_SYMBOL(pci_read_config_word);
  REGISTER_SYMBOL(pci_release_region);
  REGISTER_SYMBOL(pci_request_region);
  REGISTER_SYMBOL(pci_set_master);
  REGISTER_SYMBOL(pci_unregister_driver);
  REGISTER_SYMBOL(pci_vfs_assigned);
  REGISTER_SYMBOL(percpu_down_write);
  REGISTER_SYMBOL(percpu_free_rwsem);
  REGISTER_SYMBOL(percpu_up_write);
  REGISTER_SYMBOL(perf_trace_buf_alloc);
  REGISTER_SYMBOL(pfn_is_map_memory);
  REGISTER_SYMBOL(phy_attached_info);
  REGISTER_SYMBOL(phy_connect);
  REGISTER_SYMBOL(phy_disconnect);
  REGISTER_SYMBOL(phy_do_ioctl_running);
  REGISTER_SYMBOL(phy_print_status);
  REGISTER_SYMBOL(phy_start);
  REGISTER_SYMBOL(phy_stop);
  REGISTER_SYMBOL(phy_suspend);
  REGISTER_SYMBOL(phylink_connect_phy);
  REGISTER_SYMBOL(phylink_create);
  REGISTER_SYMBOL(phylink_destroy);
  REGISTER_SYMBOL(phylink_resume);
  REGISTER_SYMBOL(phylink_start);
  REGISTER_SYMBOL(phylink_stop);
  REGISTER_SYMBOL(phylink_suspend);
  REGISTER_SYMBOL(pid_vnr);
  REGISTER_SYMBOL(pin_user_pages);
  REGISTER_SYMBOL(pipe_lock);
  REGISTER_SYMBOL(pipe_unlock);
  REGISTER_SYMBOL(posix_clock_register);
  REGISTER_SYMBOL(ppp_channel_index);
  REGISTER_SYMBOL(ppp_dev_name);
  REGISTER_SYMBOL(ppp_input);
  REGISTER_SYMBOL(ppp_register_channel);
  REGISTER_SYMBOL(pppox_compat_ioctl);
  REGISTER_SYMBOL(pppox_ioctl);
  REGISTER_SYMBOL(pppox_unbind_sock);
  REGISTER_SYMBOL(pps_event);
  REGISTER_SYMBOL(pps_register_source);
  REGISTER_SYMBOL(pps_unregister_source);
  REGISTER_SYMBOL(proc_create_net_data);
  REGISTER_SYMBOL(proc_dointvec_jiffies);
  REGISTER_SYMBOL(proc_dointvec_minmax);
  REGISTER_SYMBOL(proto_register);
  REGISTER_SYMBOL(proto_unregister);
  REGISTER_SYMBOL(pskb_expand_head);
  REGISTER_SYMBOL(pskb_put);
  REGISTER_SYMBOL(ptp_clock_register);
  REGISTER_SYMBOL(ptp_clock_unregister);
  REGISTER_SYMBOL(put_cmsg);
  REGISTER_SYMBOL(put_pid);
  REGISTER_SYMBOL(put_user_ifreq);
  REGISTER_SYMBOL(qca_read_soc_version);
  REGISTER_SYMBOL(qca_set_bdaddr);
  REGISTER_SYMBOL(qca_set_bdaddr_rome);
  REGISTER_SYMBOL(qca_uart_setup);
  REGISTER_SYMBOL(radix_tree_tagged);
  REGISTER_SYMBOL(rb_erase);
  REGISTER_SYMBOL(rb_first);
  REGISTER_SYMBOL(rb_first_postorder);
  REGISTER_SYMBOL(rb_insert_color);
  REGISTER_SYMBOL(rb_next);
  REGISTER_SYMBOL(rb_next_postorder);
  REGISTER_SYMBOL(rcuref_get_slowpath);
  REGISTER_SYMBOL(rcuwait_wake_up);
  REGISTER_SYMBOL(recalc_sigpending);
  REGISTER_SYMBOL(refcount_dec_if_one);
  REGISTER_SYMBOL(regcache_cache_bypass);
  REGISTER_SYMBOL(regcache_cache_only);
  REGISTER_SYMBOL(regcache_drop_region);
  REGISTER_SYMBOL(regcache_mark_dirty);
  REGISTER_SYMBOL(regcache_reg_cached);
  REGISTER_SYMBOL(regcache_sync);
  REGISTER_SYMBOL(register_candev);
  REGISTER_SYMBOL(register_netdevice);
  REGISTER_SYMBOL(register_oom_notifier);
  REGISTER_SYMBOL(register_pm_notifier);
  REGISTER_SYMBOL(register_pppox_proto);
  REGISTER_SYMBOL(register_shrinker);
  REGISTER_SYMBOL(regmap_bulk_read);
  REGISTER_SYMBOL(regmap_bulk_write);
  REGISTER_SYMBOL(regmap_exit);
  REGISTER_SYMBOL(regmap_raw_read);
  REGISTER_SYMBOL(regmap_raw_write);
  REGISTER_SYMBOL(regmap_read);
  REGISTER_SYMBOL(regmap_register_patch);
  REGISTER_SYMBOL(regmap_write);
  REGISTER_SYMBOL(regulator_bulk_enable);
  REGISTER_SYMBOL(release_firmware);
  REGISTER_SYMBOL(release_sock);
  REGISTER_SYMBOL(request_firmware);
  REGISTER_SYMBOL(rfkill_alloc);
  REGISTER_SYMBOL(rfkill_blocked);
  REGISTER_SYMBOL(rfkill_destroy);
  REGISTER_SYMBOL(rfkill_register);
  REGISTER_SYMBOL(rfkill_unregister);
  REGISTER_SYMBOL(rhashtable_destroy);
  REGISTER_SYMBOL(rhashtable_init);
  REGISTER_SYMBOL(rhashtable_walk_enter);
  REGISTER_SYMBOL(rhashtable_walk_exit);
  REGISTER_SYMBOL(rhashtable_walk_next);
  REGISTER_SYMBOL(rhashtable_walk_stop);
  REGISTER_SYMBOL(rht_bucket_nested);
  REGISTER_SYMBOL(round_jiffies);
  REGISTER_SYMBOL(rtc_month_days);
  REGISTER_SYMBOL(rtl8152_get_version);
  REGISTER_SYMBOL(rtnl_configure_link);
  REGISTER_SYMBOL(rtnl_create_link);
  REGISTER_SYMBOL(rtnl_is_locked);
  REGISTER_SYMBOL(rtnl_link_register);
  REGISTER_SYMBOL(rtnl_link_unregister);
  REGISTER_SYMBOL(rtnl_lock);
  REGISTER_SYMBOL(rtnl_register_module);
  REGISTER_SYMBOL(rtnl_unlock);
  REGISTER_SYMBOL(rtnl_unregister);
  REGISTER_SYMBOL(rtnl_unregister_all);
  REGISTER_SYMBOL(schedule_timeout);
  REGISTER_SYMBOL(scnprintf);
  REGISTER_SYMBOL(sdio_claim_host);
  REGISTER_SYMBOL(sdio_claim_irq);
  REGISTER_SYMBOL(sdio_disable_func);
  REGISTER_SYMBOL(sdio_enable_func);
  REGISTER_SYMBOL(sdio_readb);
  REGISTER_SYMBOL(sdio_readsb);
  REGISTER_SYMBOL(sdio_register_driver);
  REGISTER_SYMBOL(sdio_release_host);
  REGISTER_SYMBOL(sdio_release_irq);
  REGISTER_SYMBOL(sdio_writeb);
  REGISTER_SYMBOL(sdio_writesb);
  REGISTER_SYMBOL(security_sk_clone);
  REGISTER_SYMBOL(security_sock_graft);
  REGISTER_SYMBOL(seq_hlist_next);
  REGISTER_SYMBOL(seq_hlist_start_head);
  REGISTER_SYMBOL(serdev_device_close);
  REGISTER_SYMBOL(serdev_device_open);
  REGISTER_SYMBOL(set_disk_ro);
  REGISTER_SYMBOL(set_page_private);
  REGISTER_SYMBOL(set_user_nice);
  REGISTER_SYMBOL(setup_udp_tunnel_sock);
  REGISTER_SYMBOL(sg_free_table_chained);
  REGISTER_SYMBOL(sg_init_one);
  REGISTER_SYMBOL(sg_init_table);
  REGISTER_SYMBOL(sg_nents);
  REGISTER_SYMBOL(sg_next);
  REGISTER_SYMBOL(si_mem_available);
  REGISTER_SYMBOL(si_meminfo);
  REGISTER_SYMBOL(simple_attr_open);
  REGISTER_SYMBOL(simple_attr_release);
  REGISTER_SYMBOL(simple_open);
  REGISTER_SYMBOL(sk_alloc);
  REGISTER_SYMBOL(sk_capable);
  REGISTER_SYMBOL(sk_common_release);
  REGISTER_SYMBOL(sk_error_report);
  REGISTER_SYMBOL(sk_filter_trim_cap);
  REGISTER_SYMBOL(sk_free);
  REGISTER_SYMBOL(sk_ioctl);
  REGISTER_SYMBOL(sk_msg_alloc);
  REGISTER_SYMBOL(sk_msg_clone);
  REGISTER_SYMBOL(sk_msg_free);
  REGISTER_SYMBOL(sk_msg_free_nocharge);
  REGISTER_SYMBOL(sk_msg_free_partial);
  REGISTER_SYMBOL(sk_msg_recvmsg);
  REGISTER_SYMBOL(sk_msg_return_zero);
  REGISTER_SYMBOL(sk_msg_trim);
  REGISTER_SYMBOL(sk_psock_drop);
  REGISTER_SYMBOL(sk_psock_msg_verdict);
  REGISTER_SYMBOL(sk_reset_timer);
  REGISTER_SYMBOL(sk_setup_caps);
  REGISTER_SYMBOL(sk_stop_timer);
  REGISTER_SYMBOL(sk_stream_error);
  REGISTER_SYMBOL(sk_stream_wait_memory);
  REGISTER_SYMBOL(skb_add_rx_frag);
  REGISTER_SYMBOL(skb_checksum_help);
  REGISTER_SYMBOL(skb_clone);
  REGISTER_SYMBOL(skb_copy);
  REGISTER_SYMBOL(skb_copy_bits);
  REGISTER_SYMBOL(skb_copy_expand);
  REGISTER_SYMBOL(skb_copy_header);
  REGISTER_SYMBOL(skb_cow_data);
  REGISTER_SYMBOL(skb_dequeue);
  REGISTER_SYMBOL(skb_free_datagram);
  REGISTER_SYMBOL(skb_pull);
  REGISTER_SYMBOL(skb_pull_data);
  REGISTER_SYMBOL(skb_pull_rcsum);
  REGISTER_SYMBOL(skb_push);
  REGISTER_SYMBOL(skb_put);
  REGISTER_SYMBOL(skb_queue_head);
  REGISTER_SYMBOL(skb_queue_tail);
  REGISTER_SYMBOL(skb_realloc_headroom);
  REGISTER_SYMBOL(skb_recv_datagram);
  REGISTER_SYMBOL(skb_scrub_packet);
  REGISTER_SYMBOL(skb_set_owner_w);
  REGISTER_SYMBOL(skb_splice_bits);
  REGISTER_SYMBOL(skb_to_sgvec);
  REGISTER_SYMBOL(skb_trim);
  REGISTER_SYMBOL(skb_try_coalesce);
  REGISTER_SYMBOL(skb_tstamp_tx);
  REGISTER_SYMBOL(skb_unlink);
  REGISTER_SYMBOL(skip_spaces);
  REGISTER_SYMBOL(slhc_compress);
  REGISTER_SYMBOL(slhc_free);
  REGISTER_SYMBOL(slhc_init);
  REGISTER_SYMBOL(slhc_remember);
  REGISTER_SYMBOL(slhc_toss);
  REGISTER_SYMBOL(slhc_uncompress);
  REGISTER_SYMBOL(snd_soc_add_component);
  REGISTER_SYMBOL(snd_soc_register_card);
  REGISTER_SYMBOL(sock_alloc_file);
  REGISTER_SYMBOL(sock_alloc_send_pskb);
  REGISTER_SYMBOL(sock_cmsg_send);
  REGISTER_SYMBOL(sock_common_recvmsg);
  REGISTER_SYMBOL(sock_create_kern);
  REGISTER_SYMBOL(sock_diag_register);
  REGISTER_SYMBOL(sock_diag_save_cookie);
  REGISTER_SYMBOL(sock_diag_unregister);
  REGISTER_SYMBOL(sock_efree);
  REGISTER_SYMBOL(sock_gettstamp);
  REGISTER_SYMBOL(sock_i_ino);
  REGISTER_SYMBOL(sock_i_uid);
  REGISTER_SYMBOL(sock_init_data);
  REGISTER_SYMBOL(sock_no_accept);
  REGISTER_SYMBOL(sock_no_bind);
  REGISTER_SYMBOL(sock_no_connect);
  REGISTER_SYMBOL(sock_no_getname);
  REGISTER_SYMBOL(sock_no_ioctl);
  REGISTER_SYMBOL(sock_no_listen);
  REGISTER_SYMBOL(sock_no_mmap);
  REGISTER_SYMBOL(sock_no_recvmsg);
  REGISTER_SYMBOL(sock_no_sendmsg);
  REGISTER_SYMBOL(sock_no_shutdown);
  REGISTER_SYMBOL(sock_no_socketpair);
  REGISTER_SYMBOL(sock_recv_errqueue);
  REGISTER_SYMBOL(sock_recvmsg);
  REGISTER_SYMBOL(sock_register);
  REGISTER_SYMBOL(sock_release);
  REGISTER_SYMBOL(sock_rfree);
  REGISTER_SYMBOL(sock_unregister);
  REGISTER_SYMBOL(sock_wmalloc);
  REGISTER_SYMBOL(sockfd_lookup);
  REGISTER_SYMBOL(static_key_slow_dec);
  REGISTER_SYMBOL(static_key_slow_inc);
  REGISTER_SYMBOL(string_get_size);
  REGISTER_SYMBOL(strlcat);
  REGISTER_SYMBOL(strscpy_pad);
  REGISTER_SYMBOL(strsep);
  REGISTER_SYMBOL(submit_bio_wait);
  REGISTER_SYMBOL(sync_blockdev);
  REGISTER_SYMBOL(synchronize_irq);
  REGISTER_SYMBOL(synchronize_net);
  REGISTER_SYMBOL(synchronize_srcu);
  REGISTER_SYMBOL(sysctl_vals);
  REGISTER_SYMBOL(sysfs_create_bin_file);
  REGISTER_SYMBOL(sysfs_create_file_ns);
  REGISTER_SYMBOL(sysfs_remove_bin_file);
  REGISTER_SYMBOL(sysfs_streq);
  REGISTER_SYMBOL(system_freezable_wq);
  REGISTER_SYMBOL(system_long_wq);
  REGISTER_SYMBOL(tasklet_kill);
  REGISTER_SYMBOL(tasklet_setup);
  REGISTER_SYMBOL(tasklet_unlock_wait);
  REGISTER_SYMBOL(tcp_bpf_sendmsg_redir);
  REGISTER_SYMBOL(tcp_memory_pressure);
  REGISTER_SYMBOL(tcp_poll);
  REGISTER_SYMBOL(tcp_read_done);
  REGISTER_SYMBOL(tcp_read_sock);
  REGISTER_SYMBOL(tcp_recv_skb);
  REGISTER_SYMBOL(tcp_register_ulp);
  REGISTER_SYMBOL(tcp_sendmsg_locked);
  REGISTER_SYMBOL(tcp_unregister_ulp);
  REGISTER_SYMBOL(timecounter_cyc2time);
  REGISTER_SYMBOL(timecounter_init);
  REGISTER_SYMBOL(timecounter_read);
  REGISTER_SYMBOL(timer_delete_sync);
  REGISTER_SYMBOL(timer_shutdown_sync);
  REGISTER_SYMBOL(tipc_dump_done);
  REGISTER_SYMBOL(tipc_dump_start);
  REGISTER_SYMBOL(tipc_nl_sk_walk);
  REGISTER_SYMBOL(trace_event_printf);
  REGISTER_SYMBOL(trace_event_raw_init);
  REGISTER_SYMBOL(trace_event_reg);
  REGISTER_SYMBOL(trace_handle_return);
  REGISTER_SYMBOL(trace_output_call);
  REGISTER_SYMBOL(trace_raw_output_prep);
  REGISTER_SYMBOL(tty_driver_kref_put);
  REGISTER_SYMBOL(tty_encode_baud_rate);
  REGISTER_SYMBOL(tty_flip_buffer_push);
  REGISTER_SYMBOL(tty_get_char_size);
  REGISTER_SYMBOL(tty_hangup);
  REGISTER_SYMBOL(tty_kref_put);
  REGISTER_SYMBOL(tty_ldisc_deref);
  REGISTER_SYMBOL(tty_ldisc_flush);
  REGISTER_SYMBOL(tty_ldisc_ref);
  REGISTER_SYMBOL(tty_mode_ioctl);
  REGISTER_SYMBOL(tty_port_close);
  REGISTER_SYMBOL(tty_port_destroy);
  REGISTER_SYMBOL(tty_port_hangup);
  REGISTER_SYMBOL(tty_port_init);
  REGISTER_SYMBOL(tty_port_install);
  REGISTER_SYMBOL(tty_port_open);
  REGISTER_SYMBOL(tty_port_put);
  REGISTER_SYMBOL(tty_port_tty_get);
  REGISTER_SYMBOL(tty_port_tty_hangup);
  REGISTER_SYMBOL(tty_port_tty_wakeup);
  REGISTER_SYMBOL(tty_register_driver);
  REGISTER_SYMBOL(tty_register_ldisc);
  REGISTER_SYMBOL(tty_set_termios);
  REGISTER_SYMBOL(tty_standard_install);
  REGISTER_SYMBOL(tty_std_termios);
  REGISTER_SYMBOL(tty_termios_baud_rate);
  REGISTER_SYMBOL(tty_termios_copy_hw);
  REGISTER_SYMBOL(tty_unregister_device);
  REGISTER_SYMBOL(tty_unregister_driver);
  REGISTER_SYMBOL(tty_unregister_ldisc);
  REGISTER_SYMBOL(tty_unthrottle);
  REGISTER_SYMBOL(tty_vhangup);
  REGISTER_SYMBOL(tty_wakeup);
  REGISTER_SYMBOL(udp6_set_csum);
  REGISTER_SYMBOL(udp_set_csum);
  REGISTER_SYMBOL(udp_sock_create4);
  REGISTER_SYMBOL(udp_sock_create6);
  REGISTER_SYMBOL(udp_tunnel6_xmit_skb);
  REGISTER_SYMBOL(udp_tunnel_xmit_skb);
  REGISTER_SYMBOL(unlock_page);
  REGISTER_SYMBOL(unpin_user_pages);
  REGISTER_SYMBOL(unregister_candev);
  REGISTER_SYMBOL(unregister_shrinker);
  REGISTER_SYMBOL(up_read);
  REGISTER_SYMBOL(up_write);
  REGISTER_SYMBOL(usb_alloc_coherent);
  REGISTER_SYMBOL(usb_anchor_urb);
  REGISTER_SYMBOL(usb_bus_idr);
  REGISTER_SYMBOL(usb_bus_idr_lock);
  REGISTER_SYMBOL(usb_clear_halt);
  REGISTER_SYMBOL(usb_control_msg_recv);
  REGISTER_SYMBOL(usb_control_msg_send);
  REGISTER_SYMBOL(usb_debug_root);
  REGISTER_SYMBOL(usb_disabled);
  REGISTER_SYMBOL(usb_enable_lpm);
  REGISTER_SYMBOL(usb_free_coherent);
  REGISTER_SYMBOL(usb_get_dev);
  REGISTER_SYMBOL(usb_get_from_anchor);
  REGISTER_SYMBOL(usb_get_intf);
  REGISTER_SYMBOL(usb_get_urb);
  REGISTER_SYMBOL(usb_ifnum_to_if);
  REGISTER_SYMBOL(usb_interrupt_msg);
  REGISTER_SYMBOL(usb_match_id);
  REGISTER_SYMBOL(usb_match_one_id);
  REGISTER_SYMBOL(usb_mon_deregister);
  REGISTER_SYMBOL(usb_mon_register);
  REGISTER_SYMBOL(usb_poison_urb);
  REGISTER_SYMBOL(usb_put_dev);
  REGISTER_SYMBOL(usb_put_intf);
  REGISTER_SYMBOL(usb_register_notify);
  REGISTER_SYMBOL(usb_reset_device);
  REGISTER_SYMBOL(usb_set_configuration);
  REGISTER_SYMBOL(usb_set_interface);
  REGISTER_SYMBOL(usb_show_dynids);
  REGISTER_SYMBOL(usb_store_new_id);
  REGISTER_SYMBOL(usb_string);
  REGISTER_SYMBOL(usb_unlink_urb);
  REGISTER_SYMBOL(usb_unpoison_urb);
  REGISTER_SYMBOL(usb_unregister_notify);
  REGISTER_SYMBOL(usbnet_cdc_bind);
  REGISTER_SYMBOL(usbnet_cdc_status);
  REGISTER_SYMBOL(usbnet_cdc_unbind);
  REGISTER_SYMBOL(usbnet_change_mtu);
  REGISTER_SYMBOL(usbnet_defer_kevent);
  REGISTER_SYMBOL(usbnet_disconnect);
  REGISTER_SYMBOL(usbnet_get_drvinfo);
  REGISTER_SYMBOL(usbnet_get_endpoints);
  REGISTER_SYMBOL(usbnet_get_link);
  REGISTER_SYMBOL(usbnet_get_msglevel);
  REGISTER_SYMBOL(usbnet_link_change);
  REGISTER_SYMBOL(usbnet_manage_power);
  REGISTER_SYMBOL(usbnet_nway_reset);
  REGISTER_SYMBOL(usbnet_open);
  REGISTER_SYMBOL(usbnet_probe);
  REGISTER_SYMBOL(usbnet_read_cmd);
  REGISTER_SYMBOL(usbnet_read_cmd_nopm);
  REGISTER_SYMBOL(usbnet_resume);
  REGISTER_SYMBOL(usbnet_set_msglevel);
  REGISTER_SYMBOL(usbnet_set_rx_mode);
  REGISTER_SYMBOL(usbnet_skb_return);
  REGISTER_SYMBOL(usbnet_start_xmit);
  REGISTER_SYMBOL(usbnet_stop);
  REGISTER_SYMBOL(usbnet_suspend);
  REGISTER_SYMBOL(usbnet_tx_timeout);
  REGISTER_SYMBOL(usbnet_unlink_rx_urbs);
  REGISTER_SYMBOL(usbnet_write_cmd);
  REGISTER_SYMBOL(usbnet_write_cmd_nopm);
  REGISTER_SYMBOL(usleep_range_state);
  REGISTER_SYMBOL(v9fs_register_trans);
  REGISTER_SYMBOL(v9fs_unregister_trans);
  REGISTER_SYMBOL(virtio_break_device);
  REGISTER_SYMBOL(virtio_config_changed);
  REGISTER_SYMBOL(virtio_device_freeze);
  REGISTER_SYMBOL(virtio_device_restore);
  REGISTER_SYMBOL(virtio_max_dma_size);
  REGISTER_SYMBOL(virtio_reset_device);
  REGISTER_SYMBOL(virtqueue_add_inbuf);
  REGISTER_SYMBOL(virtqueue_add_outbuf);
  REGISTER_SYMBOL(virtqueue_add_sgs);
  REGISTER_SYMBOL(virtqueue_disable_cb);
  REGISTER_SYMBOL(virtqueue_enable_cb);
  REGISTER_SYMBOL(virtqueue_get_buf);
  REGISTER_SYMBOL(virtqueue_is_broken);
  REGISTER_SYMBOL(virtqueue_kick);
  REGISTER_SYMBOL(virtqueue_notify);
  REGISTER_SYMBOL(vlan_dev_vlan_id);
  REGISTER_SYMBOL(vlan_filter_drop_vids);
  REGISTER_SYMBOL(vlan_filter_push_vids);
  REGISTER_SYMBOL(vlan_ioctl_set);
  REGISTER_SYMBOL(vlan_uses_dev);
  REGISTER_SYMBOL(vlan_vid_add);
  REGISTER_SYMBOL(vlan_vid_del);
  REGISTER_SYMBOL(vm_event_states);
  REGISTER_SYMBOL(vm_iomap_memory);
  REGISTER_SYMBOL(vm_node_stat);
  REGISTER_SYMBOL(vp_legacy_get_status);
  REGISTER_SYMBOL(vp_legacy_probe);
  REGISTER_SYMBOL(vp_legacy_remove);
  REGISTER_SYMBOL(vp_legacy_set_status);
  REGISTER_SYMBOL(vp_modern_generation);
  REGISTER_SYMBOL(vp_modern_get_status);
  REGISTER_SYMBOL(vp_modern_probe);
  REGISTER_SYMBOL(vp_modern_remove);
  REGISTER_SYMBOL(vp_modern_set_status);
  REGISTER_SYMBOL(vring_del_virtqueue);
  REGISTER_SYMBOL(vring_interrupt);
  REGISTER_SYMBOL(vscnprintf);
  REGISTER_SYMBOL(vsock_core_register);
  REGISTER_SYMBOL(vsock_core_unregister);
  REGISTER_SYMBOL(wait_woken);
  REGISTER_SYMBOL(wake_up_bit);
  REGISTER_SYMBOL(wake_up_process);
  REGISTER_SYMBOL(woken_wake_function);
  REGISTER_SYMBOL(work_busy);
  REGISTER_SYMBOL(wpan_phy_free);
  REGISTER_SYMBOL(wpan_phy_new);
  REGISTER_SYMBOL(wpan_phy_register);
  REGISTER_SYMBOL(wpan_phy_unregister);
  REGISTER_SYMBOL(zlib_deflate);
  REGISTER_SYMBOL(zlib_deflateEnd);
  REGISTER_SYMBOL(zlib_deflateInit2);
  REGISTER_SYMBOL(zlib_deflateReset);
  REGISTER_SYMBOL(zlib_inflate);
  REGISTER_SYMBOL(zlib_inflateIncomp);
  REGISTER_SYMBOL(zlib_inflateInit2);
  REGISTER_SYMBOL(zlib_inflateReset);
  REGISTER_SYMBOL(zs_compact);
  REGISTER_SYMBOL(zs_create_pool);
  REGISTER_SYMBOL(zs_destroy_pool);
  REGISTER_SYMBOL(zs_free);
  REGISTER_SYMBOL(zs_get_total_pages);
  REGISTER_SYMBOL(zs_huge_class_size);
  REGISTER_SYMBOL(zs_lookup_class_index);
  REGISTER_SYMBOL(zs_malloc);
  REGISTER_SYMBOL(zs_map_object);
  REGISTER_SYMBOL(zs_pool_stats);
  REGISTER_SYMBOL(zs_unmap_object);


  REGISTER_SYMBOL(__clk_mux_determine_rate_closest);
  REGISTER_SYMBOL(__cpuhp_state_add_instance);
  REGISTER_SYMBOL(__cpuhp_state_remove_instance);
  REGISTER_SYMBOL(__dev_change_net_namespace);
  REGISTER_SYMBOL(__ethtool_get_link_ksettings);
  REGISTER_SYMBOL(__get_random_u32_below);
  REGISTER_SYMBOL(__init_swait_queue_head);
  REGISTER_SYMBOL(__irq_apply_affinity_hint);
  REGISTER_SYMBOL(__kmalloc_node_track_caller);
  REGISTER_SYMBOL(__kunit_activate_static_stub);
  REGISTER_SYMBOL(__mmap_lock_do_trace_acquire_returned);
  REGISTER_SYMBOL(__mmap_lock_do_trace_released);
  REGISTER_SYMBOL(__mmap_lock_do_trace_start_locking);
  REGISTER_SYMBOL(__module_put_and_kthread_exit);
  REGISTER_SYMBOL(__ndisc_fill_addr_option);
  REGISTER_SYMBOL(__page_pinner_put_page);
  REGISTER_SYMBOL(__platform_driver_probe);
  REGISTER_SYMBOL(__pm_runtime_set_status);
  REGISTER_SYMBOL(__pm_runtime_use_autosuspend);
  REGISTER_SYMBOL(__root_device_register);
  REGISTER_SYMBOL(__serdev_device_driver_register);
  REGISTER_SYMBOL(__sock_recv_wifi_status);
  REGISTER_SYMBOL(__trace_trigger_soft_disabled);
  REGISTER_SYMBOL(__traceiter_android_vh_gzvm_destroy_vm_post_process);
  REGISTER_SYMBOL(__traceiter_android_vh_gzvm_handle_demand_page_post);
  REGISTER_SYMBOL(__traceiter_android_vh_gzvm_handle_demand_page_pre);
  REGISTER_SYMBOL(__traceiter_android_vh_gzvm_vcpu_exit_reason);
  REGISTER_SYMBOL(__traceiter_android_vh_zs_shrinker_adjust);
  REGISTER_SYMBOL(__traceiter_android_vh_zs_shrinker_bypass);
  REGISTER_SYMBOL(__traceiter_sk_data_ready);
  REGISTER_SYMBOL(__tracepoint_android_vh_gzvm_destroy_vm_post_process);
  REGISTER_SYMBOL(__tracepoint_android_vh_gzvm_handle_demand_page_post);
  REGISTER_SYMBOL(__tracepoint_android_vh_gzvm_handle_demand_page_pre);
  REGISTER_SYMBOL(__tracepoint_android_vh_gzvm_vcpu_exit_reason);
  REGISTER_SYMBOL(__tracepoint_android_vh_zs_shrinker_adjust);
  REGISTER_SYMBOL(__tracepoint_android_vh_zs_shrinker_bypass);
  REGISTER_SYMBOL(__tracepoint_mmap_lock_acquire_returned);
  REGISTER_SYMBOL(__tracepoint_mmap_lock_released);
  REGISTER_SYMBOL(__tracepoint_mmap_lock_start_locking);
  REGISTER_SYMBOL(__tracepoint_rwmmio_post_read);
  REGISTER_SYMBOL(__tracepoint_rwmmio_post_write);
  REGISTER_SYMBOL(__tracepoint_rwmmio_read);
  REGISTER_SYMBOL(__tracepoint_rwmmio_write);
  REGISTER_SYMBOL(__tracepoint_sk_data_ready);
  REGISTER_SYMBOL(__tty_insert_flip_string_flags);
  REGISTER_SYMBOL(_snd_pcm_hw_params_any);
  REGISTER_SYMBOL(_trace_android_vh_record_pcpu_rwsem_starttime);
  REGISTER_SYMBOL(add_wait_queue_exclusive);
  REGISTER_SYMBOL(add_wait_queue_priority);
  REGISTER_SYMBOL(addrconf_add_linklocal);
  REGISTER_SYMBOL(addrconf_prefix_rcv_add_addr);
  REGISTER_SYMBOL(adjust_managed_page_count);
  REGISTER_SYMBOL(bio_end_io_acct_remapped);
  REGISTER_SYMBOL(blk_mq_complete_request_remote);
  REGISTER_SYMBOL(blk_mq_end_request_batch);
  REGISTER_SYMBOL(blk_mq_quiesce_queue_nowait);
  REGISTER_SYMBOL(blk_mq_unquiesce_queue);
  REGISTER_SYMBOL(blk_mq_virtio_map_queues);
  REGISTER_SYMBOL(blk_queue_alignment_offset);
  REGISTER_SYMBOL(blk_queue_chunk_sectors);
  REGISTER_SYMBOL(blk_queue_max_discard_sectors);
  REGISTER_SYMBOL(blk_queue_max_discard_segments);
  REGISTER_SYMBOL(blk_queue_max_secure_erase_sectors);
  REGISTER_SYMBOL(blk_queue_max_write_zeroes_sectors);
  REGISTER_SYMBOL(blk_queue_max_zone_append_sectors);
  REGISTER_SYMBOL(blk_revalidate_disk_zones);
  REGISTER_SYMBOL(bt_sock_reclassify_lock);
  REGISTER_SYMBOL(bt_sock_stream_recvmsg);
  REGISTER_SYMBOL(btbcm_read_pcm_int_params);
  REGISTER_SYMBOL(btbcm_write_pcm_int_params);
  REGISTER_SYMBOL(call_netdevice_notifiers);
  REGISTER_SYMBOL(can_dropped_invalid_skb);
  REGISTER_SYMBOL(clk_hw_determine_rate_no_reparent);
  REGISTER_SYMBOL(clk_hw_get_num_parents);
  REGISTER_SYMBOL(clk_hw_init_rate_request);
  REGISTER_SYMBOL(clk_notifier_unregister);
  REGISTER_SYMBOL(clocks_calc_mult_shift);
  REGISTER_SYMBOL(crypto_aead_setauthsize);
  REGISTER_SYMBOL(crypto_alloc_sync_skcipher);
  REGISTER_SYMBOL(crypto_comp_decompress);
  REGISTER_SYMBOL(crypto_ecdh_encode_key);
  REGISTER_SYMBOL(crypto_get_default_rng);
  REGISTER_SYMBOL(crypto_put_default_rng);
  REGISTER_SYMBOL(crypto_shash_tfm_digest);
  REGISTER_SYMBOL(crypto_skcipher_decrypt);
  REGISTER_SYMBOL(crypto_skcipher_encrypt);
  REGISTER_SYMBOL(crypto_skcipher_setkey);
  REGISTER_SYMBOL(dev_kfree_skb_any_reason);
  REGISTER_SYMBOL(dev_kfree_skb_irq_reason);
  REGISTER_SYMBOL(device_find_child_by_name);
  REGISTER_SYMBOL(device_for_each_child_reverse);
  REGISTER_SYMBOL(device_property_present);
  REGISTER_SYMBOL(device_property_read_string);
  REGISTER_SYMBOL(device_property_read_u32_array);
  REGISTER_SYMBOL(device_property_read_u8_array);
  REGISTER_SYMBOL(device_set_wakeup_capable);
  REGISTER_SYMBOL(device_set_wakeup_enable);
  REGISTER_SYMBOL(devm_clk_get_optional_enabled);
  REGISTER_SYMBOL(devm_platform_get_and_ioremap_resource);
  REGISTER_SYMBOL(devm_platform_ioremap_resource);
  REGISTER_SYMBOL(devm_regulator_bulk_get);
  REGISTER_SYMBOL(do_trace_netlink_extack);
  REGISTER_SYMBOL(eth_platform_get_mac_address);
  REGISTER_SYMBOL(ethtool_convert_legacy_u32_to_link_mode);
  REGISTER_SYMBOL(ethtool_convert_link_mode_to_legacy_u32);
  REGISTER_SYMBOL(ethtool_op_get_ts_info);
  REGISTER_SYMBOL(eventfd_ctx_remove_wait_queue);
  REGISTER_SYMBOL(firmware_request_nowarn);
  REGISTER_SYMBOL(fwnode_property_read_u8_array);
  REGISTER_SYMBOL(generic_hwtstamp_get_lower);
  REGISTER_SYMBOL(generic_hwtstamp_set_lower);
  REGISTER_SYMBOL(genl_unregister_family);
  REGISTER_SYMBOL(get_device_system_crosststamp);
  REGISTER_SYMBOL(gpiochip_add_data_with_key);
  REGISTER_SYMBOL(gpiod_get_value_cansleep);
  REGISTER_SYMBOL(gpiod_set_value_cansleep);
  REGISTER_SYMBOL(hci_devcd_append_pattern);
  REGISTER_SYMBOL(hrtimer_start_range_ns);
  REGISTER_SYMBOL(ieee802154_beacon_push);
  REGISTER_SYMBOL(ieee802154_hdr_peek_addrs);
  REGISTER_SYMBOL(ieee802154_mac_cmd_pl_pull);
  REGISTER_SYMBOL(ieee802154_mac_cmd_push);
  REGISTER_SYMBOL(ieee802154_max_payload);
  REGISTER_SYMBOL(inet_frag_queue_insert);
  REGISTER_SYMBOL(inet_frag_reasm_finish);
  REGISTER_SYMBOL(inet_frag_reasm_prepare);
  REGISTER_SYMBOL(iov_iter_extract_pages);
  REGISTER_SYMBOL(kmem_cache_create_usercopy);
  REGISTER_SYMBOL(kthread_cancel_delayed_work_sync);
  REGISTER_SYMBOL(kthread_complete_and_exit);
  REGISTER_SYMBOL(kthread_create_on_node);
  REGISTER_SYMBOL(kthread_delayed_work_timer_fn);
  REGISTER_SYMBOL(kthread_destroy_worker);
  REGISTER_SYMBOL(kthread_mod_delayed_work);
  REGISTER_SYMBOL(kthread_queue_delayed_work);
  REGISTER_SYMBOL(ktime_get_mono_fast_ns);
  REGISTER_SYMBOL(kunit_deactivate_static_stub);
  REGISTER_SYMBOL(kunit_destroy_resource);
  REGISTER_SYMBOL(kunit_mem_assert_format);
  REGISTER_SYMBOL(kvm_arch_ptp_get_crosststamp);
  REGISTER_SYMBOL(kvm_arm_hyp_service_available);
  REGISTER_SYMBOL(l2tp_session_dec_refcount);
  REGISTER_SYMBOL(l2tp_session_inc_refcount);
  REGISTER_SYMBOL(l2tp_session_set_header_len);
  REGISTER_SYMBOL(l2tp_tunnel_dec_refcount);
  REGISTER_SYMBOL(l2tp_tunnel_get_session);
  REGISTER_SYMBOL(l2tp_tunnel_inc_refcount);
  REGISTER_SYMBOL(lowpan_header_compress);
  REGISTER_SYMBOL(lowpan_header_decompress);
  REGISTER_SYMBOL(lowpan_register_netdevice);
  REGISTER_SYMBOL(lowpan_unregister_netdevice);
  REGISTER_SYMBOL(memcg_sockets_enabled_key);
  REGISTER_SYMBOL(memory_cgrp_subsys_on_dfl_key);
  REGISTER_SYMBOL(mii_ethtool_get_link_ksettings);
  REGISTER_SYMBOL(mii_ethtool_set_link_ksettings);
  REGISTER_SYMBOL(mutex_lock_interruptible);
  REGISTER_SYMBOL(net_selftest_get_count);
  REGISTER_SYMBOL(net_selftest_get_strings);
  REGISTER_SYMBOL(netdev_rx_handler_register);
  REGISTER_SYMBOL(netdev_rx_handler_unregister);
  REGISTER_SYMBOL(netdev_update_features);
  REGISTER_SYMBOL(netdev_upper_dev_unlink);
  REGISTER_SYMBOL(netif_set_tso_max_size);
  REGISTER_SYMBOL(netif_stacked_transfer_operstate);
  REGISTER_SYMBOL(netlink_register_notifier);
  REGISTER_SYMBOL(netlink_unregister_notifier);
  REGISTER_SYMBOL(nl802154_beaconing_done);
  REGISTER_SYMBOL(ns_to_kernel_old_timeval);
  REGISTER_SYMBOL(of_find_node_opts_by_path);
  REGISTER_SYMBOL(of_property_read_string_helper);
  REGISTER_SYMBOL(of_property_read_variable_u32_array);
  REGISTER_SYMBOL(of_reserved_mem_lookup);
  REGISTER_SYMBOL(out_of_line_wait_on_bit);
  REGISTER_SYMBOL(out_of_line_wait_on_bit_timeout);
  REGISTER_SYMBOL(page_reporting_register);
  REGISTER_SYMBOL(page_reporting_unregister);
  REGISTER_SYMBOL(pci_alloc_irq_vectors_affinity);
  REGISTER_SYMBOL(pci_find_ext_capability);
  REGISTER_SYMBOL(pci_find_next_capability);
  REGISTER_SYMBOL(pci_release_selected_regions);
  REGISTER_SYMBOL(pci_request_selected_regions);
  REGISTER_SYMBOL(perf_trace_run_bpf_submit);
  REGISTER_SYMBOL(phy_ethtool_get_link_ksettings);
  REGISTER_SYMBOL(phy_ethtool_nway_reset);
  REGISTER_SYMBOL(phy_ethtool_set_link_ksettings);
  REGISTER_SYMBOL(phylink_disconnect_phy);
  REGISTER_SYMBOL(phylink_ethtool_get_pauseparam);
  REGISTER_SYMBOL(phylink_ethtool_set_pauseparam);
  REGISTER_SYMBOL(posix_clock_unregister);
  REGISTER_SYMBOL(post_page_relinquish_tlb_inv);
  REGISTER_SYMBOL(ppp_register_compressor);
  REGISTER_SYMBOL(ppp_register_net_channel);
  REGISTER_SYMBOL(ppp_unregister_channel);
  REGISTER_SYMBOL(ppp_unregister_compressor);
  REGISTER_SYMBOL(proc_create_net_single);
  REGISTER_SYMBOL(proc_create_seq_private);
  REGISTER_SYMBOL(proc_doulongvec_minmax);
  REGISTER_SYMBOL(qca_send_pre_shutdown_cmd);
  REGISTER_SYMBOL(register_module_notifier);
  REGISTER_SYMBOL(register_net_sysctl_sz);
  REGISTER_SYMBOL(register_netdevice_notifier);
  REGISTER_SYMBOL(register_pernet_device);
  REGISTER_SYMBOL(register_pernet_subsys);
  REGISTER_SYMBOL(register_virtio_device);
  REGISTER_SYMBOL(register_virtio_driver);
  REGISTER_SYMBOL(regulator_bulk_disable);
  REGISTER_SYMBOL(rhashtable_insert_slow);
  REGISTER_SYMBOL(rhashtable_walk_start_check);
  REGISTER_SYMBOL(rht_bucket_nested_insert);
  REGISTER_SYMBOL(root_device_unregister);
  REGISTER_SYMBOL(schedule_timeout_interruptible);
  REGISTER_SYMBOL(schedule_timeout_uninterruptible);
  REGISTER_SYMBOL(sdio_unregister_driver);
  REGISTER_SYMBOL(security_release_secctx);
  REGISTER_SYMBOL(security_secid_to_secctx);
  REGISTER_SYMBOL(security_sk_classify_flow);
  REGISTER_SYMBOL(serdev_device_get_tiocm);
  REGISTER_SYMBOL(serdev_device_set_baudrate);
  REGISTER_SYMBOL(serdev_device_set_flow_control);
  REGISTER_SYMBOL(serdev_device_set_tiocm);
  REGISTER_SYMBOL(serdev_device_wait_until_sent);
  REGISTER_SYMBOL(serdev_device_write_buf);
  REGISTER_SYMBOL(serdev_device_write_flush);
  REGISTER_SYMBOL(set_capacity_and_notify);
  REGISTER_SYMBOL(set_normalized_timespec64);
  REGISTER_SYMBOL(sg_alloc_table_chained);
  REGISTER_SYMBOL(simple_read_from_buffer);
  REGISTER_SYMBOL(sk_msg_memcopy_from_iter);
  REGISTER_SYMBOL(sk_msg_zerocopy_from_iter);
  REGISTER_SYMBOL(sk_psock_tls_strp_read);
  REGISTER_SYMBOL(skb_copy_datagram_iter);
  REGISTER_SYMBOL(skb_queue_purge_reason);
  REGISTER_SYMBOL(snd_soc_component_initialize);
  REGISTER_SYMBOL(snd_soc_params_to_bclk);
  REGISTER_SYMBOL(snd_soc_tdm_params_to_bclk);
  REGISTER_SYMBOL(snd_soc_tplg_component_load);
  REGISTER_SYMBOL(snd_soc_tplg_component_remove);
  REGISTER_SYMBOL(snd_soc_unregister_card);
  REGISTER_SYMBOL(snd_soc_unregister_component);
  REGISTER_SYMBOL(sock_common_getsockopt);
  REGISTER_SYMBOL(sock_common_setsockopt);
  REGISTER_SYMBOL(sock_queue_rcv_skb_reason);
  REGISTER_SYMBOL(tcp_rate_check_app_limited);
  REGISTER_SYMBOL(tipc_sk_fill_sock_diag);
  REGISTER_SYMBOL(trace_event_buffer_commit);
  REGISTER_SYMBOL(trace_event_buffer_reserve);
  REGISTER_SYMBOL(trace_print_symbols_seq);
  REGISTER_SYMBOL(tty_driver_flush_buffer);
  REGISTER_SYMBOL(tty_port_register_device);
  REGISTER_SYMBOL(tty_termios_encode_baud_rate);
  REGISTER_SYMBOL(udp_tunnel_sock_release);
  REGISTER_SYMBOL(unpin_user_pages_dirty_lock);
  REGISTER_SYMBOL(unregister_module_notifier);
  REGISTER_SYMBOL(unregister_net_sysctl_table);
  REGISTER_SYMBOL(unregister_netdevice_many);
  REGISTER_SYMBOL(unregister_netdevice_notifier);
  REGISTER_SYMBOL(unregister_netdevice_queue);
  REGISTER_SYMBOL(unregister_oom_notifier);
  REGISTER_SYMBOL(unregister_pernet_device);
  REGISTER_SYMBOL(unregister_pernet_subsys);
  REGISTER_SYMBOL(unregister_pm_notifier);
  REGISTER_SYMBOL(unregister_pppox_proto);
  REGISTER_SYMBOL(unregister_virtio_device);
  REGISTER_SYMBOL(unregister_virtio_driver);
  REGISTER_SYMBOL(usb_altnum_to_altsetting);
  REGISTER_SYMBOL(usb_autopm_get_interface);
  REGISTER_SYMBOL(usb_autopm_get_interface_async);
  REGISTER_SYMBOL(usb_autopm_get_interface_no_resume);
  REGISTER_SYMBOL(usb_autopm_put_interface);
  REGISTER_SYMBOL(usb_autopm_put_interface_async);
  REGISTER_SYMBOL(usb_check_bulk_endpoints);
  REGISTER_SYMBOL(usb_check_int_endpoints);
  REGISTER_SYMBOL(usb_deregister_device_driver);
  REGISTER_SYMBOL(usb_driver_claim_interface);
  REGISTER_SYMBOL(usb_driver_release_interface);
  REGISTER_SYMBOL(usb_driver_set_configuration);
  REGISTER_SYMBOL(usb_find_common_endpoints);
  REGISTER_SYMBOL(usb_queue_reset_device);
  REGISTER_SYMBOL(usb_register_device_driver);
  REGISTER_SYMBOL(usb_reset_configuration);
  REGISTER_SYMBOL(usb_serial_deregister_drivers);
  REGISTER_SYMBOL(usb_serial_generic_get_icount);
  REGISTER_SYMBOL(usb_serial_generic_open);
  REGISTER_SYMBOL(usb_serial_generic_throttle);
  REGISTER_SYMBOL(usb_serial_generic_tiocmiwait);
  REGISTER_SYMBOL(usb_serial_generic_unthrottle);
  REGISTER_SYMBOL(usb_serial_handle_dcd_change);
  REGISTER_SYMBOL(usb_serial_register_drivers);
  REGISTER_SYMBOL(usbnet_cdc_update_filter);
  REGISTER_SYMBOL(usbnet_device_suggests_idle);
  REGISTER_SYMBOL(usbnet_get_ethernet_addr);
  REGISTER_SYMBOL(usbnet_get_link_ksettings_internal);
  REGISTER_SYMBOL(usbnet_get_link_ksettings_mii);
  REGISTER_SYMBOL(usbnet_set_link_ksettings_mii);
  REGISTER_SYMBOL(usbnet_update_max_qlen);
  REGISTER_SYMBOL(usbnet_write_cmd_async);
  REGISTER_SYMBOL(virtio_check_driver_offered_feature);
  REGISTER_SYMBOL(virtio_transport_connect);
  REGISTER_SYMBOL(virtio_transport_deliver_tap_pkt);
  REGISTER_SYMBOL(virtio_transport_destruct);
  REGISTER_SYMBOL(virtio_transport_dgram_allow);
  REGISTER_SYMBOL(virtio_transport_dgram_bind);
  REGISTER_SYMBOL(virtio_transport_dgram_dequeue);
  REGISTER_SYMBOL(virtio_transport_dgram_enqueue);
  REGISTER_SYMBOL(virtio_transport_do_socket_init);
  REGISTER_SYMBOL(virtio_transport_notify_buffer_size);
  REGISTER_SYMBOL(virtio_transport_notify_poll_in);
  REGISTER_SYMBOL(virtio_transport_notify_poll_out);
  REGISTER_SYMBOL(virtio_transport_notify_recv_init);
  REGISTER_SYMBOL(virtio_transport_notify_recv_post_dequeue);
  REGISTER_SYMBOL(virtio_transport_notify_recv_pre_block);
  REGISTER_SYMBOL(virtio_transport_notify_recv_pre_dequeue);
  REGISTER_SYMBOL(virtio_transport_notify_send_init);
  REGISTER_SYMBOL(virtio_transport_notify_send_post_enqueue);
  REGISTER_SYMBOL(virtio_transport_notify_send_pre_block);
  REGISTER_SYMBOL(virtio_transport_notify_send_pre_enqueue);
  REGISTER_SYMBOL(virtio_transport_notify_set_rcvlowat);
  REGISTER_SYMBOL(virtio_transport_purge_skbs);
  REGISTER_SYMBOL(virtio_transport_read_skb);
  REGISTER_SYMBOL(virtio_transport_recv_pkt);
  REGISTER_SYMBOL(virtio_transport_release);
  REGISTER_SYMBOL(virtio_transport_seqpacket_dequeue);
  REGISTER_SYMBOL(virtio_transport_seqpacket_enqueue);
  REGISTER_SYMBOL(virtio_transport_seqpacket_has_data);
  REGISTER_SYMBOL(virtio_transport_shutdown);
  REGISTER_SYMBOL(virtio_transport_stream_allow);
  REGISTER_SYMBOL(virtio_transport_stream_dequeue);
  REGISTER_SYMBOL(virtio_transport_stream_enqueue);
  REGISTER_SYMBOL(virtio_transport_stream_has_data);
  REGISTER_SYMBOL(virtio_transport_stream_has_space);
  REGISTER_SYMBOL(virtio_transport_stream_is_active);
  REGISTER_SYMBOL(virtio_transport_stream_rcvhiwat);
  REGISTER_SYMBOL(virtqueue_detach_unused_buf);
  REGISTER_SYMBOL(virtqueue_disable_dma_api_for_buffers);
  REGISTER_SYMBOL(virtqueue_get_avail_addr);
  REGISTER_SYMBOL(virtqueue_get_desc_addr);
  REGISTER_SYMBOL(virtqueue_get_used_addr);
  REGISTER_SYMBOL(virtqueue_get_vring_size);
  REGISTER_SYMBOL(virtqueue_kick_prepare);
  REGISTER_SYMBOL(vp_legacy_config_vector);
  REGISTER_SYMBOL(vp_legacy_get_features);
  REGISTER_SYMBOL(vp_legacy_get_queue_enable);
  REGISTER_SYMBOL(vp_legacy_get_queue_size);
  REGISTER_SYMBOL(vp_legacy_queue_vector);
  REGISTER_SYMBOL(vp_legacy_set_features);
  REGISTER_SYMBOL(vp_legacy_set_queue_address);
  REGISTER_SYMBOL(vp_modern_config_vector);
  REGISTER_SYMBOL(vp_modern_get_features);
  REGISTER_SYMBOL(vp_modern_get_num_queues);
  REGISTER_SYMBOL(vp_modern_get_queue_enable);
  REGISTER_SYMBOL(vp_modern_get_queue_reset);
  REGISTER_SYMBOL(vp_modern_get_queue_size);
  REGISTER_SYMBOL(vp_modern_map_vq_notify);
  REGISTER_SYMBOL(vp_modern_queue_address);
  REGISTER_SYMBOL(vp_modern_queue_vector);
  REGISTER_SYMBOL(vp_modern_set_features);
  REGISTER_SYMBOL(vp_modern_set_queue_enable);
  REGISTER_SYMBOL(vp_modern_set_queue_reset);
  REGISTER_SYMBOL(vp_modern_set_queue_size);
  REGISTER_SYMBOL(vring_create_virtqueue);
  REGISTER_SYMBOL(vring_notification_data);
  REGISTER_SYMBOL(vring_transport_features);
  REGISTER_SYMBOL(vsock_for_each_connected_socket);
  REGISTER_SYMBOL(zlib_deflate_workspacesize);
  REGISTER_SYMBOL(zlib_inflate_workspacesize);

  fprintf(stderr, "driverhub: registered %zu KMI symbols\n", symbols_.size());
}

#undef REGISTER_SYMBOL

}  // namespace driverhub
