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
#include "src/shim/subsystem/gpio.h"
#include "src/shim/subsystem/i2c.h"
#include "src/shim/subsystem/spi.h"
#include "src/shim/subsystem/usb.h"

// Forward declarations for RTC, timer, and ABI-compat shim symbols.
extern "C" {
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

  fprintf(stderr, "driverhub: registered %zu KMI symbols\n", symbols_.size());
}

#undef REGISTER_SYMBOL

}  // namespace driverhub
