// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_IRQ_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_IRQ_H_

// KMI shims for Linux kernel IRQ handling APIs.
//
// Provides: request_irq, free_irq, request_threaded_irq,
//           disable_irq, enable_irq, irq_set_irq_type
//
// On Fuchsia: maps to zx_interrupt_wait via IRQ FIDL protocol.
// In userspace shim: IRQs are simulated — request_irq records the handler,
// free_irq removes it. No actual hardware interrupts are delivered.

#ifdef __cplusplus
extern "C" {
#endif

typedef int irqreturn_t;
#define IRQ_NONE      0
#define IRQ_HANDLED   1
#define IRQ_WAKE_THREAD 2

typedef irqreturn_t (*irq_handler_t)(int irq, void *dev_id);

// IRQ flags.
#define IRQF_SHARED       0x00000080
#define IRQF_TRIGGER_RISING  0x00000001
#define IRQF_TRIGGER_FALLING 0x00000002
#define IRQF_TRIGGER_HIGH    0x00000004
#define IRQF_TRIGGER_LOW     0x00000008
#define IRQF_ONESHOT         0x00002000

int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char *name, void *dev_id);

int request_threaded_irq(unsigned int irq, irq_handler_t handler,
                         irq_handler_t thread_fn, unsigned long flags,
                         const char *name, void *dev_id);

void free_irq(unsigned int irq, void *dev_id);

void disable_irq(unsigned int irq);
void enable_irq(unsigned int irq);
void disable_irq_nosync(unsigned int irq);

// Device-managed IRQ request.
int devm_request_irq(struct device *dev, unsigned int irq,
                     irq_handler_t handler, unsigned long flags,
                     const char *name, void *dev_id);

int devm_request_threaded_irq(struct device *dev, unsigned int irq,
                              irq_handler_t handler, irq_handler_t thread_fn,
                              unsigned long flags, const char *name,
                              void *dev_id);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_IRQ_H_
