// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Composite Node Demo
//
// Proves that a consumer driver .ko can bind to GPIO pins provided by a
// GPIO controller .ko through the DriverHub service bridge. This is the
// key test for composite node assembly:
//
//   1. Load the GPIO controller module → registers gpio_chip with 32 pins
//      → service bridge creates per-pin DFv2 child nodes
//   2. Load the touchscreen module → requests GPIO pins 3 (IRQ) and 8 (reset)
//      → uses pins through integer GPIO API
//      → performs reset sequence: assert low, deassert high
//   3. Verify that:
//      a. Both modules loaded and initialized successfully
//      b. GPIO chip was registered and bridge notified
//      c. Touchscreen probe succeeded (pins accessible cross-module)
//      d. Pin states are correct after reset sequence
//      e. IRQ mapping works through the GPIO controller
//      f. Clean shutdown works for both modules
//
// On Fuchsia, the composite node would be assembled by the driver manager:
//   - touchscreen.cml declares: use fuchsia.hardware.gpio.Service
//   - bind rules match: gpio-3 and gpio-8 from driverhub-gpio controller
//   - FIDL calls route through GpioServiceServer → gpio_chip callbacks

#include <cstdio>
#include <cstring>

#include "src/bus_driver/bus_driver.h"
#include "src/fuchsia/service_bridge.h"

#if defined(__Fuchsia__)
#include "src/fuchsia/resource_provider.h"
#endif

// Forward declarations for gpio query APIs (in gpio.cc shim).
extern "C" {
int driverhub_gpiochip_count(void);
struct gpio_chip* driverhub_gpiochip_get(int index);
int gpio_get_value(unsigned gpio);
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
          "  DriverHub Composite Node Demo\n"
          "  GPIO Controller + Touchscreen Consumer\n"
          "============================================================\n\n");

  if (argc < 3) {
    fprintf(stderr,
            "usage: %s <gpio_controller.ko> <touchscreen.ko>\n\n"
            "Demonstrates composite node binding:\n"
            "  1. GPIO controller registers gpio_chip → service bridge\n"
            "  2. Touchscreen driver requests GPIO pins → cross-module\n"
            "  3. Verifies pin state through the composite binding\n",
            argv[0]);
    return 1;
  }

  const char* gpio_ko_path = argv[1];
  const char* ts_ko_path = argv[2];

#if defined(__Fuchsia__)
  fprintf(stderr, "[composite] Running on Fuchsia\n");
  dh_resources_init();
#else
  fprintf(stderr, "[composite] Running on host (userspace simulation)\n");
#endif

  // -----------------------------------------------------------------------
  // Phase 1: Init bus driver + load GPIO controller
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 1: Load GPIO Controller ---\n"
          "[composite] Loading %s\n",
          gpio_ko_path);

  driverhub::BusDriver bus;
  auto status = bus.Init();
  if (status != 0) {
    fprintf(stderr, "[composite] FATAL: bus init failed: %d\n", status);
    return 1;
  }

  status = bus.LoadModuleFromFile(gpio_ko_path);
  if (status != 0) {
    fprintf(stderr, "[composite] ERROR: GPIO module load failed: %d\n",
            status);
    bus.Shutdown();
    return 1;
  }

  bool gpio_loaded = true;
  fprintf(stderr, "[composite] GPIO controller module loaded\n");

  // -----------------------------------------------------------------------
  // Phase 2: Verify GPIO chip and bridge notification
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 2: Verify GPIO Service Bridge ---\n");

  int chip_count = driverhub_gpiochip_count();
  fprintf(stderr, "[composite] Registered GPIO chips: %d\n", chip_count);

  bool chip_ok = chip_count >= 1;
  struct gpio_chip* gc = chip_ok ? driverhub_gpiochip_get(0) : nullptr;

  bool bridge_ok = false;
  if (gc) {
    // Check that the service bridge can find this chip.
    struct gpio_chip* found = dh_bridge_find_gpio_chip(gc->base);
    bridge_ok = (found == gc);
    fprintf(stderr,
            "[composite] Bridge lookup for base %d: %s\n",
            gc->base, bridge_ok ? "found" : "NOT FOUND");

    // Also check per-pin lookup.
    struct gpio_chip* pin3 = dh_bridge_find_gpio_chip(gc->base + 3);
    struct gpio_chip* pin8 = dh_bridge_find_gpio_chip(gc->base + 8);
    fprintf(stderr,
            "[composite] Bridge lookup for pin 3: %s\n",
            pin3 == gc ? "found" : "NOT FOUND");
    fprintf(stderr,
            "[composite] Bridge lookup for pin 8: %s\n",
            pin8 == gc ? "found" : "NOT FOUND");
    bridge_ok = bridge_ok && (pin3 == gc) && (pin8 == gc);
  }

  fprintf(stderr,
          "[composite] chip='%s' base=%d ngpio=%u\n",
          gc ? (gc->label ? gc->label : "(null)") : "N/A",
          gc ? gc->base : -1,
          gc ? gc->ngpio : 0);

  // -----------------------------------------------------------------------
  // Phase 3: Load touchscreen consumer (composite driver)
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 3: Load Touchscreen Composite Driver ---\n"
          "[composite] Loading %s\n"
          "[composite] This driver will request GPIO pins 3 (IRQ) and "
          "8 (reset)\n"
          "[composite] from the GPIO controller loaded in Phase 1.\n\n",
          ts_ko_path);

  status = bus.LoadModuleFromFile(ts_ko_path);
  bool ts_loaded = (status == 0);

  if (!ts_loaded) {
    fprintf(stderr, "[composite] ERROR: touchscreen module load failed: %d\n",
            status);
  } else {
    fprintf(stderr, "[composite] Touchscreen module loaded and probed\n");
  }

  // -----------------------------------------------------------------------
  // Phase 4: Verify cross-module GPIO state
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 4: Verify Cross-Module Pin State ---\n");

  // The touchscreen module should have:
  //   - Requested GPIO 3 as input (for IRQ)
  //   - Requested GPIO 8 as output, low (reset assert), then set high
  //
  // Verify pin states through the GPIO controller's callbacks.
  bool pin_state_ok = false;
  bool irq_ok = false;
  bool reset_ok = false;

  if (gc && ts_loaded) {
    // Check pin 3 direction (should be input = 1).
    int pin3_dir = gc->get_direction ? gc->get_direction(gc, 3) : -1;
    fprintf(stderr, "[composite] Pin 3 direction: %d (1=input)\n", pin3_dir);

    // Check pin 3 IRQ mapping.
    int pin3_irq = gc->to_irq ? gc->to_irq(gc, 3) : -1;
    fprintf(stderr, "[composite] Pin 3 IRQ: %d\n", pin3_irq);
    irq_ok = (pin3_dir == 1 && pin3_irq > 0);

    // Check pin 8 value (should be 1 after deassert).
    int pin8_val = gc->get ? gc->get(gc, 8) : -1;
    int pin8_dir = gc->get_direction ? gc->get_direction(gc, 8) : -1;
    fprintf(stderr, "[composite] Pin 8 direction: %d (0=output)\n", pin8_dir);
    fprintf(stderr, "[composite] Pin 8 value: %d (1=deasserted)\n", pin8_val);
    reset_ok = (pin8_dir == 0 && pin8_val == 1);

    pin_state_ok = irq_ok && reset_ok;
  }

  // Also verify via the integer GPIO API (global state table).
  bool global_state_ok = false;
  if (ts_loaded) {
    int g3 = gpio_get_value(3);
    int g8 = gpio_get_value(8);
    fprintf(stderr, "[composite] Global GPIO state: pin3=%d pin8=%d\n",
            g3, g8);
    // Pin 3 is input (value 0 default), pin 8 should be 1 (deasserted).
    global_state_ok = (g8 == 1);
  }

  // -----------------------------------------------------------------------
  // Phase 5: Verify module count
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 5: Module Topology ---\n");

  size_t mod_count = bus.module_count();
  fprintf(stderr,
          "[composite] Loaded modules: %zu\n"
          "[composite]   Module 1: GPIO controller (provider)\n"
          "[composite]   Module 2: Touchscreen (consumer)\n",
          mod_count);

  bool topology_ok = (mod_count == 2);

  // -----------------------------------------------------------------------
  // Phase 6: Shutdown
  // -----------------------------------------------------------------------
  fprintf(stderr, "\n--- Phase 6: Shutdown ---\n");
  bus.Shutdown();

  // -----------------------------------------------------------------------
  // Results
  // -----------------------------------------------------------------------
  bool all_pass = gpio_loaded && chip_ok && bridge_ok && ts_loaded &&
                  irq_ok && reset_ok && pin_state_ok &&
                  global_state_ok && topology_ok;

  fprintf(stderr,
          "\n============================================================\n"
          "  Composite Node Demo Results\n"
          "============================================================\n"
          "  GPIO controller loaded:     %s\n"
          "  GPIO chip registered:       %s (%d chip, %u pins)\n"
          "  Service bridge notified:    %s\n"
          "  Touchscreen loaded:         %s\n"
          "  IRQ pin (3) configured:     %s\n"
          "  Reset pin (8) sequence:     %s\n"
          "  Cross-module pin state:     %s\n"
          "  Global GPIO state:          %s\n"
          "  Module topology (2 nodes):  %s\n"
          "============================================================\n\n",
          pass_fail(gpio_loaded),
          pass_fail(chip_ok), chip_count, gc ? gc->ngpio : 0,
          pass_fail(bridge_ok),
          pass_fail(ts_loaded),
          pass_fail(irq_ok),
          pass_fail(reset_ok),
          pass_fail(pin_state_ok),
          pass_fail(global_state_ok),
          pass_fail(topology_ok));

  return all_pass ? 0 : 1;
}
