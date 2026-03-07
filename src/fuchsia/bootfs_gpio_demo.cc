// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bootfs GPIO Provider Demo
//
// Demonstrates DriverHub running from bootfs in early boot:
//   1. Loads a GPIO controller .ko module via the ELF loader
//   2. Module registers a gpio_chip with 32 pins
//   3. Shows pins available through the integer and descriptor GPIO APIs
//   4. Simulates a composite node binding scenario where another driver
//      (e.g., a touchscreen) would consume GPIO pins for IRQ + reset
//   5. Exercises gpio_chip callbacks: direction, get, set, to_irq
//   6. Clean shutdown with module_exit()
//
// On Fuchsia bootfs: runs via zircon.autorun.boot.
// Files are at /boot/ instead of /pkg/.

#include <cstdio>
#include <cstring>

#include "src/bus_driver/bus_driver.h"

#if defined(__Fuchsia__)
#include "src/fuchsia/resource_provider.h"
#endif

// Forward declarations for gpio_chip query APIs (in gpio.cc shim).
extern "C" {
int driverhub_gpiochip_count(void);
struct gpio_chip* driverhub_gpiochip_get(int index);
}

// Minimal gpio_chip forward declaration (matches shim header).
struct gpio_chip {
  const char* label;
  struct device* parent;
  struct module* owner;
  int base;
  unsigned int ngpio;

  int (*request)(struct gpio_chip*, unsigned);
  void (*free)(struct gpio_chip*, unsigned);
  int (*get_direction)(struct gpio_chip*, unsigned);
  int (*direction_input)(struct gpio_chip*, unsigned);
  int (*direction_output)(struct gpio_chip*, unsigned, int);
  int (*get)(struct gpio_chip*, unsigned);
  void (*set)(struct gpio_chip*, unsigned, int);
  int (*to_irq)(struct gpio_chip*, unsigned);

  void* _data;
};

static const char* pass_fail(bool ok) { return ok ? "PASS" : "FAIL"; }

int main(int argc, char** argv) {
  fprintf(stderr,
          "\n"
          "============================================================\n"
          "  DriverHub Bootfs GPIO Provider Demo\n"
          "============================================================\n\n");

  if (argc < 2) {
    fprintf(stderr, "usage: %s <gpio-controller.ko>\n", argv[0]);
    return 1;
  }

  const char* ko_path = argv[1];

#if defined(__Fuchsia__)
  fprintf(stderr, "[bootfs-gpio] Running on Fuchsia bootfs\n");
  dh_resources_init();
#else
  fprintf(stderr, "[bootfs-gpio] Running on host (userspace simulation)\n");
#endif

  // -----------------------------------------------------------------------
  // Phase 1: Init bus driver + load GPIO controller module
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 1: Load GPIO Controller Module ---\n"
          "[bootfs-gpio] Loading %s\n",
          ko_path);

  driverhub::BusDriver bus;
  auto status = bus.Init();
  if (status != 0) {
    fprintf(stderr, "[bootfs-gpio] FATAL: bus init failed: %d\n", status);
    return 1;
  }

  status = bus.LoadModuleFromFile(ko_path);
  if (status != 0) {
    fprintf(stderr, "[bootfs-gpio] ERROR: module load failed: %d\n", status);
    bus.Shutdown();
    return 1;
  }

  // -----------------------------------------------------------------------
  // Phase 2: Verify gpio_chip registration
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 2: Verify GPIO Chip Registration ---\n");

  int chip_count = driverhub_gpiochip_count();
  fprintf(stderr, "[bootfs-gpio] Registered GPIO chips: %d\n", chip_count);

  bool chip_ok = chip_count >= 1;
  if (!chip_ok) {
    fprintf(stderr, "[bootfs-gpio] ERROR: no GPIO chips registered!\n");
    bus.Shutdown();
    return 1;
  }

  struct gpio_chip* gc = driverhub_gpiochip_get(0);
  if (!gc) {
    fprintf(stderr, "[bootfs-gpio] ERROR: gpio_chip pointer is null!\n");
    bus.Shutdown();
    return 1;
  }

  fprintf(stderr, "[bootfs-gpio] chip[0]: label='%s' base=%d ngpio=%u\n",
          gc->label ? gc->label : "(null)", gc->base, gc->ngpio);
  fprintf(stderr, "[bootfs-gpio] chip[0] callbacks: direction_input=%p "
                  "direction_output=%p get=%p set=%p to_irq=%p\n",
          reinterpret_cast<void*>(gc->direction_input),
          reinterpret_cast<void*>(gc->direction_output),
          reinterpret_cast<void*>(gc->get),
          reinterpret_cast<void*>(gc->set),
          reinterpret_cast<void*>(gc->to_irq));

  bool has_ops = gc->direction_input && gc->direction_output &&
                 gc->get && gc->set && gc->to_irq;

  // -----------------------------------------------------------------------
  // Phase 3: Exercise GPIO chip callbacks (simulated hardware ops)
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 3: Exercise GPIO Chip Ops ---\n");

  // Pin 5: configure as output, set high, read back.
  bool pin5_ok = false;
  if (gc->direction_output && gc->get) {
    int ret = gc->direction_output(gc, 5, 1);
    int val = gc->get(gc, 5);
    fprintf(stderr, "[bootfs-gpio] pin 5: direction_output(1)=%d get()=%d\n",
            ret, val);
    pin5_ok = (ret == 0 && val == 1);
  }

  // Pin 12: configure as input, verify direction.
  bool pin12_ok = false;
  if (gc->direction_input && gc->get_direction) {
    int ret = gc->direction_input(gc, 12);
    int dir = gc->get_direction(gc, 12);
    fprintf(stderr,
            "[bootfs-gpio] pin 12: direction_input()=%d "
            "get_direction()=%d (1=in)\n",
            ret, dir);
    pin12_ok = (ret == 0 && dir == 1);
  }

  // Pin 7: set to 0, then toggle to 1, verify.
  bool pin7_toggle_ok = false;
  if (gc->direction_output && gc->set && gc->get) {
    gc->direction_output(gc, 7, 0);
    int v0 = gc->get(gc, 7);
    gc->set(gc, 7, 1);
    int v1 = gc->get(gc, 7);
    fprintf(stderr, "[bootfs-gpio] pin 7: toggle 0→1: before=%d after=%d\n",
            v0, v1);
    pin7_toggle_ok = (v0 == 0 && v1 == 1);
  }

  // -----------------------------------------------------------------------
  // Phase 4: Simulate composite node binding (IRQ + Reset GPIOs)
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 4: Composite Node GPIO Binding ---\n"
          "[bootfs-gpio] Simulating touchscreen driver composite bind:\n"
          "[bootfs-gpio]   - IRQ pin: GPIO %d (pin 3)\n"
          "[bootfs-gpio]   - Reset pin: GPIO %d (pin 8)\n",
          gc->base + 3, gc->base + 8);

  // A real Fuchsia composite node would bind against:
  //   fuchsia.hardware.gpio.Service == "driverhub-gpio/gpio-3"
  //   fuchsia.hardware.gpio.Service == "driverhub-gpio/gpio-8"
  //
  // Here we show the GPIO provider serving those pins.

  bool irq_ok = false;
  if (gc->direction_input && gc->to_irq) {
    int ret = gc->direction_input(gc, 3);
    int irq = gc->to_irq(gc, 3);
    fprintf(stderr, "[bootfs-gpio] IRQ pin (3): direction_input=%d irq=%d\n",
            ret, irq);
    irq_ok = (ret == 0 && irq > 0);
  }

  bool reset_ok = false;
  if (gc->direction_output) {
    // Reset sequence: assert low, then deassert high.
    int r1 = gc->direction_output(gc, 8, 0);
    int v1 = gc->get ? gc->get(gc, 8) : -1;
    gc->set(gc, 8, 1);
    int v2 = gc->get ? gc->get(gc, 8) : -1;
    fprintf(stderr,
            "[bootfs-gpio] Reset pin (8): assert=%d (val=%d), "
            "deassert (val=%d)\n",
            r1, v1, v2);
    reset_ok = (r1 == 0 && v1 == 0 && v2 == 1);
  }

  // -----------------------------------------------------------------------
  // Phase 5: Shutdown
  // -----------------------------------------------------------------------
  fprintf(stderr, "\n--- Phase 5: Shutdown ---\n");
  bus.Shutdown();

  // -----------------------------------------------------------------------
  // Results
  // -----------------------------------------------------------------------
  bool all_pass = chip_ok && has_ops && pin5_ok && pin12_ok &&
                  pin7_toggle_ok && irq_ok && reset_ok;

  fprintf(stderr,
          "\n============================================================\n"
          "  Bootfs GPIO Provider Results\n"
          "============================================================\n"
          "  Module loading:      %s\n"
          "  Chip registration:   %s (%d chip, %u pins)\n"
          "  Chip callbacks:      %s\n"
          "  Pin output/readback: %s\n"
          "  Pin input/direction: %s\n"
          "  Pin toggle:          %s\n"
          "  Composite IRQ pin:   %s\n"
          "  Composite reset pin: %s\n"
          "============================================================\n\n",
          "PASS",
          pass_fail(chip_ok), chip_count, gc->ngpio,
          pass_fail(has_ops),
          pass_fail(pin5_ok),
          pass_fail(pin12_ok),
          pass_fail(pin7_toggle_ok),
          pass_fail(irq_ok),
          pass_fail(reset_ok));

  return all_pass ? 0 : 1;
}
