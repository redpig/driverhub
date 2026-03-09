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
#include "src/shim/subsystem/led.h"

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

  fprintf(stderr, "driverhub: registered %zu KMI symbols\n", symbols_.size());
}

#undef REGISTER_SYMBOL

}  // namespace driverhub
