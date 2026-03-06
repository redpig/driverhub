// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/irq.h"

#include <cstdio>
#include <mutex>
#include <unordered_map>

// IRQ shim: records handlers for each IRQ number.
//
// In userspace, no real hardware interrupts are delivered. The bus driver
// or test harness can invoke registered handlers via driverhub_trigger_irq().
//
// On Fuchsia, request_irq would:
//   1. Obtain an interrupt object via the platform device FIDL protocol.
//   2. Create a dedicated thread that calls zx_interrupt_wait().
//   3. When an interrupt fires, invoke the registered handler.

namespace {

struct IrqRegistration {
  irq_handler_t handler;
  irq_handler_t thread_fn;
  unsigned long flags;
  const char* name;
  void* dev_id;
  bool enabled;
};

std::mutex g_irq_mu;
std::unordered_map<unsigned int, IrqRegistration> g_irq_handlers;

}  // namespace

extern "C" {

int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char* name, void* dev_id) {
  return request_threaded_irq(irq, handler, nullptr, flags, name, dev_id);
}

int request_threaded_irq(unsigned int irq, irq_handler_t handler,
                         irq_handler_t thread_fn, unsigned long flags,
                         const char* name, void* dev_id) {
  std::lock_guard<std::mutex> lock(g_irq_mu);

  if (g_irq_handlers.count(irq) && !(flags & IRQF_SHARED)) {
    fprintf(stderr, "driverhub: irq: IRQ %u already registered\n", irq);
    return -16;  // -EBUSY
  }

  g_irq_handlers[irq] = {handler, thread_fn, flags, name, dev_id, true};
  fprintf(stderr, "driverhub: irq: registered IRQ %u (%s)\n", irq, name);
  return 0;
}

void free_irq(unsigned int irq, void* dev_id) {
  std::lock_guard<std::mutex> lock(g_irq_mu);
  auto it = g_irq_handlers.find(irq);
  if (it != g_irq_handlers.end() && it->second.dev_id == dev_id) {
    fprintf(stderr, "driverhub: irq: freed IRQ %u (%s)\n",
            irq, it->second.name);
    g_irq_handlers.erase(it);
  }
}

void disable_irq(unsigned int irq) {
  std::lock_guard<std::mutex> lock(g_irq_mu);
  auto it = g_irq_handlers.find(irq);
  if (it != g_irq_handlers.end()) {
    it->second.enabled = false;
  }
}

void enable_irq(unsigned int irq) {
  std::lock_guard<std::mutex> lock(g_irq_mu);
  auto it = g_irq_handlers.find(irq);
  if (it != g_irq_handlers.end()) {
    it->second.enabled = true;
  }
}

void disable_irq_nosync(unsigned int irq) {
  disable_irq(irq);
}

int devm_request_irq(struct device* dev, unsigned int irq,
                     irq_handler_t handler, unsigned long flags,
                     const char* name, void* dev_id) {
  (void)dev;  // TODO: Track for automatic cleanup.
  return request_irq(irq, handler, flags, name, dev_id);
}

int devm_request_threaded_irq(struct device* dev, unsigned int irq,
                              irq_handler_t handler, irq_handler_t thread_fn,
                              unsigned long flags, const char* name,
                              void* dev_id) {
  (void)dev;
  return request_threaded_irq(irq, handler, thread_fn, flags, name, dev_id);
}

}  // extern "C"

// Test helper: trigger an IRQ handler from the test harness.
extern "C" irqreturn_t driverhub_trigger_irq(unsigned int irq, void* dev_id) {
  std::lock_guard<std::mutex> lock(g_irq_mu);
  auto it = g_irq_handlers.find(irq);
  if (it == g_irq_handlers.end() || !it->second.enabled) {
    return IRQ_NONE;
  }
  return it->second.handler(irq, dev_id ? dev_id : it->second.dev_id);
}
