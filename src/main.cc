// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CLI harness for testing DriverHub module loading on the host.
//
// Usage: driverhub <module1.ko> [module2.ko] ...
//
// Initializes the KMI shim layer, loads each .ko module in order, calls
// module_init(), and then calls module_exit() on shutdown.

#include <cstdio>
#include <cstdlib>

#include "src/bus_driver/bus_driver.h"
#include "src/shim/subsystem/rfkill_server.h"

#if defined(__Fuchsia__)
#include "src/fuchsia/resource_provider.h"
#endif

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <module.ko> [module2.ko ...]\n", argv[0]);
    return 1;
  }

#if defined(__Fuchsia__)
  // Acquire Fuchsia kernel resources for hardware access.
  dh_resources_init();
#endif

  driverhub::BusDriver bus;

  auto status = bus.Init();
  if (status != 0) {
    fprintf(stderr, "fatal: bus driver init failed: %d\n", status);
    return 1;
  }

  // Load each module specified on the command line.
  for (int i = 1; i < argc; i++) {
    fprintf(stderr, "\n--- Loading %s ---\n", argv[i]);
    status = bus.LoadModuleFromFile(argv[i]);
    if (status != 0) {
      fprintf(stderr, "error: failed to load %s (status=%d)\n",
              argv[i], status);
      // Continue loading other modules.
    }
  }

  fprintf(stderr, "\n--- %zu module(s) loaded ---\n", bus.module_count());

  // Start the rfkill IPC server so rfkillctl can query/control radios.
  driverhub::StartRfkillServer();

  fprintf(stderr, "Press Enter to unload and exit...\n");
  getchar();

  driverhub::StopRfkillServer();
  bus.Shutdown();
  return 0;
}
