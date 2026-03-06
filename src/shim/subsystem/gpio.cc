// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/gpio.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

// GPIO subsystem shim.
//
// Maintains a simulated GPIO state table. In userspace, all GPIOs succeed
// and maintain their set/get values in a simple map.
//
// On Fuchsia, each GPIO would be backed by a fuchsia.hardware.gpio/Gpio
// FIDL connection obtained from the incoming namespace.

// Define the opaque gpio_desc type (forward-declared in gpio.h).
struct gpio_desc {
  unsigned gpio;
};

namespace {

struct GpioState {
  bool requested = false;
  bool is_output = false;
  int value = 0;
  const char* label = nullptr;
};

std::mutex g_gpio_mu;
std::unordered_map<unsigned, GpioState> g_gpios;
std::unordered_map<unsigned, struct gpio_desc> g_gpio_descs;

}  // namespace

extern "C" {

int gpio_is_valid(int number) {
  return number >= 0 && number < 1024;
}

int gpio_request(unsigned gpio, const char* label) {
  std::lock_guard<std::mutex> lock(g_gpio_mu);
  auto& state = g_gpios[gpio];
  if (state.requested) {
    fprintf(stderr, "driverhub: gpio: GPIO %u already requested\n", gpio);
    return -16;  // -EBUSY
  }
  state.requested = true;
  state.label = label;
  fprintf(stderr, "driverhub: gpio: requested GPIO %u (%s)\n",
          gpio, label ? label : "");
  return 0;
}

void gpio_free(unsigned gpio) {
  std::lock_guard<std::mutex> lock(g_gpio_mu);
  g_gpios.erase(gpio);
  fprintf(stderr, "driverhub: gpio: freed GPIO %u\n", gpio);
}

int gpio_direction_input(unsigned gpio) {
  std::lock_guard<std::mutex> lock(g_gpio_mu);
  g_gpios[gpio].is_output = false;
  return 0;
}

int gpio_direction_output(unsigned gpio, int value) {
  std::lock_guard<std::mutex> lock(g_gpio_mu);
  g_gpios[gpio].is_output = true;
  g_gpios[gpio].value = value;
  return 0;
}

int gpio_get_value(unsigned gpio) {
  std::lock_guard<std::mutex> lock(g_gpio_mu);
  auto it = g_gpios.find(gpio);
  return (it != g_gpios.end()) ? it->second.value : 0;
}

void gpio_set_value(unsigned gpio, int value) {
  std::lock_guard<std::mutex> lock(g_gpio_mu);
  g_gpios[gpio].value = value;
}

int gpio_to_irq(unsigned gpio) {
  // Simulated: IRQ number = GPIO number + 256 offset.
  return static_cast<int>(gpio + 256);
}

int devm_gpio_request(struct device* dev, unsigned gpio, const char* label) {
  (void)dev;
  return gpio_request(gpio, label);
}

int devm_gpio_request_one(struct device* dev, unsigned gpio,
                          unsigned long flags, const char* label) {
  int ret = devm_gpio_request(dev, gpio, label);
  if (ret) return ret;

  if (flags & GPIOF_DIR_IN) {
    return gpio_direction_input(gpio);
  } else {
    int value = (flags & GPIOF_INIT_HIGH) ? 1 : 0;
    return gpio_direction_output(gpio, value);
  }
}

// --- GPIO descriptor API ---

struct gpio_desc* gpiod_get(struct device* dev, const char* con_id,
                            unsigned long flags) {
  (void)dev;
  // Simplified: allocate a GPIO descriptor with an auto-assigned number.
  static unsigned next_gpio = 512;
  unsigned gpio = next_gpio++;

  std::lock_guard<std::mutex> lock(g_gpio_mu);
  g_gpios[gpio] = {true, false, 0, con_id};
  g_gpio_descs[gpio] = {gpio};

  fprintf(stderr, "driverhub: gpio: gpiod_get('%s') -> GPIO %u\n",
          con_id ? con_id : "", gpio);
  return &g_gpio_descs[gpio];
}

struct gpio_desc* gpiod_get_optional(struct device* dev, const char* con_id,
                                     unsigned long flags) {
  return gpiod_get(dev, con_id, flags);
}

struct gpio_desc* devm_gpiod_get(struct device* dev, const char* con_id,
                                 unsigned long flags) {
  return gpiod_get(dev, con_id, flags);
}

struct gpio_desc* devm_gpiod_get_optional(struct device* dev,
                                          const char* con_id,
                                          unsigned long flags) {
  return gpiod_get(dev, con_id, flags);
}

void gpiod_put(struct gpio_desc* desc) {
  if (!desc) return;
  gpio_free(desc->gpio);
}

int gpiod_direction_input(struct gpio_desc* desc) {
  return desc ? gpio_direction_input(desc->gpio) : -22;
}

int gpiod_direction_output(struct gpio_desc* desc, int value) {
  return desc ? gpio_direction_output(desc->gpio, value) : -22;
}

int gpiod_get_value(const struct gpio_desc* desc) {
  return desc ? gpio_get_value(desc->gpio) : 0;
}

void gpiod_set_value(struct gpio_desc* desc, int value) {
  if (desc) gpio_set_value(desc->gpio, value);
}

int gpiod_to_irq(const struct gpio_desc* desc) {
  return desc ? gpio_to_irq(desc->gpio) : -22;
}

}  // extern "C"
