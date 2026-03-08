// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Native Composite Node Demo
//
// Proves that a Fuchsia-native DFv2 driver can consume GPIO pins provided
// by a .ko module loaded through DriverHub's service bridge.
//
// This is the full end-to-end story:
//
//   1. Load gpio_controller_module.ko → registers gpio_chip → service bridge
//      creates per-pin DFv2 child nodes offering fuchsia.hardware.gpio.Service
//
//   2. Simulate a native DFv2 driver (GpioLedDriver) binding to gpio-5:
//      - ConfigOut(0) → gc->direction_output(gc, 5, 0)
//      - Write(1)     → gc->set(gc, 5, 1)
//      - Read()       → gc->get(gc, 5) = 1
//      - Write(0)     → gc->set(gc, 5, 0)
//      - Read()       → gc->get(gc, 5) = 0
//
//   3. Verify that FIDL-level operations route through to the .ko module's
//      gpio_chip callbacks correctly.
//
// On Fuchsia, the GpioLedDriver would be a separate DFv2 component that
// binds via composite node spec. Here on host, we simulate the FIDL calls
// by calling the GpioServiceServer methods directly (or the gpio_chip ops).

#include <cstdio>
#include <cstring>

#include "src/bus_driver/bus_driver.h"
#include "src/fuchsia/service_bridge.h"

#if defined(__Fuchsia__)
#include "src/fuchsia/resource_provider.h"
#endif

extern "C" {
int driverhub_gpiochip_count(void);
struct gpio_chip* driverhub_gpiochip_get(int index);
}

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

// Simulates what GpioServiceServer does on Fuchsia: routes FIDL calls
// through to the gpio_chip's callback functions.
namespace fidl_sim {

struct GpioClient {
  struct gpio_chip* chip;
  unsigned offset;
};

int ConfigOut(GpioClient* c, int initial_value) {
  if (!c->chip || !c->chip->direction_output) return -1;
  return c->chip->direction_output(c->chip, c->offset, initial_value);
}

int ConfigIn(GpioClient* c) {
  if (!c->chip || !c->chip->direction_input) return -1;
  return c->chip->direction_input(c->chip, c->offset);
}

int Write(GpioClient* c, int value) {
  if (!c->chip || !c->chip->set) return -1;
  c->chip->set(c->chip, c->offset, value);
  return 0;
}

int Read(GpioClient* c) {
  if (!c->chip || !c->chip->get) return -1;
  return c->chip->get(c->chip, c->offset);
}

int GetPin(GpioClient* c) {
  return c->chip->base + static_cast<int>(c->offset);
}

}  // namespace fidl_sim

int main(int argc, char** argv) {
  fprintf(stderr,
          "\n"
          "============================================================\n"
          "  DriverHub Native Composite Node Demo\n"
          "  Fuchsia DFv2 Driver ←→ .ko GPIO Controller\n"
          "============================================================\n\n");

  if (argc < 2) {
    fprintf(stderr,
            "usage: %s <gpio_controller.ko>\n\n"
            "Demonstrates a native Fuchsia DFv2 driver consuming GPIO\n"
            "pins from a .ko module through the service bridge.\n",
            argv[0]);
    return 1;
  }

  const char* gpio_ko_path = argv[1];

#if defined(__Fuchsia__)
  fprintf(stderr, "[native-composite] Running on Fuchsia\n");
  dh_resources_init();
#else
  fprintf(stderr, "[native-composite] Running on host (userspace simulation)\n");
#endif

  // -----------------------------------------------------------------------
  // Phase 1: Load GPIO controller .ko module
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 1: Load GPIO Controller .ko Module ---\n"
          "[native-composite] Loading %s\n",
          gpio_ko_path);

  driverhub::BusDriver bus;
  auto status = bus.Init();
  if (status != 0) {
    fprintf(stderr, "[native-composite] FATAL: bus init failed: %d\n", status);
    return 1;
  }

  status = bus.LoadModuleFromFile(gpio_ko_path);
  if (status != 0) {
    fprintf(stderr,
            "[native-composite] ERROR: GPIO module load failed: %d\n",
            status);
    bus.Shutdown();
    return 1;
  }

  bool ko_loaded = true;
  fprintf(stderr, "[native-composite] GPIO controller .ko loaded\n");

  // -----------------------------------------------------------------------
  // Phase 2: Verify GPIO chip registered and bridge notified
  // -----------------------------------------------------------------------
  fprintf(stderr, "\n--- Phase 2: Verify Service Bridge ---\n");

  int chip_count = driverhub_gpiochip_count();
  bool chip_ok = chip_count >= 1;
  struct gpio_chip* gc = chip_ok ? driverhub_gpiochip_get(0) : nullptr;

  bool bridge_ok = false;
  if (gc) {
    struct gpio_chip* found = dh_bridge_find_gpio_chip(gc->base + 5);
    bridge_ok = (found == gc);
    fprintf(stderr,
            "[native-composite] Bridge lookup for pin 5: %s\n",
            bridge_ok ? "found" : "NOT FOUND");
  }

  fprintf(stderr,
          "[native-composite] chip='%s' base=%d ngpio=%u\n",
          gc ? (gc->label ? gc->label : "(null)") : "N/A",
          gc ? gc->base : -1, gc ? gc->ngpio : 0);

  // -----------------------------------------------------------------------
  // Phase 3: Simulate native DFv2 driver connecting via FIDL
  // -----------------------------------------------------------------------
  fprintf(stderr,
          "\n--- Phase 3: Native Driver Binds to gpio-5 ---\n"
          "[native-composite] Simulating GpioLedDriver connecting to\n"
          "[native-composite] fuchsia.hardware.gpio.Service on gpio-5\n"
          "[native-composite] (backed by .ko gpio_chip callbacks)\n\n");

  // This simulates what happens on Fuchsia:
  //   1. Driver Manager sees gpio-5 child node offers fuchsia.hardware.gpio.Service
  //   2. GpioLedDriver's bind rules match
  //   3. Driver Manager starts GpioLedDriver with gpio-5's service in its namespace
  //   4. GpioLedDriver calls incoming()->Connect<fuchsia.hardware.gpio.Service>()
  //   5. FIDL calls route through GpioServiceServer → gpio_chip callbacks

  fidl_sim::GpioClient client = {gc, 5};

  // Step 1: ConfigOut(0) — configure as output, initial value 0 (LED off)
  fprintf(stderr, "[gpio-led] ConfigOut(0) → direction_output(5, 0)\n");
  int ret = fidl_sim::ConfigOut(&client, 0);
  bool config_ok = (ret == 0);
  fprintf(stderr, "[gpio-led]   result: %d (%s)\n", ret, config_ok ? "ok" : "error");

  // Step 2: Read() — verify pin reads 0
  int val = fidl_sim::Read(&client);
  fprintf(stderr, "[gpio-led] Read() → get(5) = %d\n", val);
  bool read0_ok = (val == 0);

  // Step 3: Write(1) — turn LED on
  fprintf(stderr, "[gpio-led] Write(1) → set(5, 1)\n");
  ret = fidl_sim::Write(&client, 1);
  bool write1_ok = (ret == 0);

  // Step 4: Read() — verify pin reads 1
  val = fidl_sim::Read(&client);
  fprintf(stderr, "[gpio-led] Read() → get(5) = %d\n", val);
  bool read1_ok = (val == 1);

  // Step 5: Write(0) — turn LED off
  fprintf(stderr, "[gpio-led] Write(0) → set(5, 0)\n");
  ret = fidl_sim::Write(&client, 0);
  bool write0_ok = (ret == 0);

  // Step 6: Read() — verify pin reads 0 again
  val = fidl_sim::Read(&client);
  fprintf(stderr, "[gpio-led] Read() → get(5) = %d\n", val);
  bool read_final_ok = (val == 0);

  // Step 7: GetPin() — verify pin number
  int pin_num = fidl_sim::GetPin(&client);
  fprintf(stderr, "[gpio-led] GetPin() = %d\n", pin_num);
  bool pin_ok = (pin_num == 5);

  // Step 8: ConfigIn() — reconfigure as input
  fprintf(stderr, "[gpio-led] ConfigIn() → direction_input(5)\n");
  ret = fidl_sim::ConfigIn(&client);
  bool input_ok = (ret == 0);

  // Step 9: Verify direction changed
  int dir = gc ? (gc->get_direction ? gc->get_direction(gc, 5) : -1) : -1;
  fprintf(stderr, "[gpio-led] get_direction(5) = %d (1=input)\n", dir);
  bool dir_ok = (dir == 1);

  // -----------------------------------------------------------------------
  // Phase 4: Module topology
  // -----------------------------------------------------------------------
  fprintf(stderr, "\n--- Phase 4: DFv2 Topology ---\n");

  fprintf(stderr,
          "[native-composite] Loaded modules: %zu\n"
          "[native-composite]   .ko: gpio_controller_module (provider)\n"
          "[native-composite]   DFv2: gpio-led-driver (native consumer)\n",
          bus.module_count());

  fprintf(stderr,
          "[native-composite] Service path:\n"
          "[native-composite]   gpio-led-driver\n"
          "[native-composite]     → fuchsia.hardware.gpio.Service/default\n"
          "[native-composite]     → GpioServiceServer(chip=%s, offset=5)\n"
          "[native-composite]     → gpio_chip->direction_output / set / get\n"
          "[native-composite]     → gpio_controller_module.ko callbacks\n",
          gc ? (gc->label ? gc->label : "(null)") : "N/A");

  // -----------------------------------------------------------------------
  // Phase 5: Shutdown
  // -----------------------------------------------------------------------
  fprintf(stderr, "\n--- Phase 5: Shutdown ---\n");
  bus.Shutdown();

  // -----------------------------------------------------------------------
  // Results
  // -----------------------------------------------------------------------
  bool fidl_ok = config_ok && read0_ok && write1_ok && read1_ok &&
                 write0_ok && read_final_ok;
  bool all_pass = ko_loaded && chip_ok && bridge_ok && fidl_ok &&
                  pin_ok && input_ok && dir_ok;

  fprintf(stderr,
          "\n============================================================\n"
          "  Native Composite Node Demo Results\n"
          "============================================================\n"
          "  .ko module loaded:           %s\n"
          "  GPIO chip registered:        %s (%d chip, %u pins)\n"
          "  Service bridge (pin 5):      %s\n"
          "  FIDL ConfigOut(0):           %s\n"
          "  FIDL Read() = 0:             %s\n"
          "  FIDL Write(1) + Read() = 1:  %s\n"
          "  FIDL Write(0) + Read() = 0:  %s\n"
          "  FIDL GetPin() = 5:           %s\n"
          "  FIDL ConfigIn():             %s\n"
          "  Direction verify (input):    %s\n"
          "============================================================\n\n",
          pass_fail(ko_loaded),
          pass_fail(chip_ok), chip_count, gc ? gc->ngpio : 0,
          pass_fail(bridge_ok),
          pass_fail(config_ok),
          pass_fail(read0_ok),
          pass_fail(write1_ok && read1_ok),
          pass_fail(write0_ok && read_final_ok),
          pass_fail(pin_ok),
          pass_fail(input_ok),
          pass_fail(dir_ok));

  return all_pass ? 0 : 1;
}
