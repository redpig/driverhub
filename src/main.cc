// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CLI harness for testing DriverHub module loading on the host.
//
// Usage: driverhub [--manifest <file>] [module1.ko] [module2.ko] ...
//
// Initializes the KMI shim layer, loads each .ko module in order, calls
// module_init(), and then calls module_exit() on shutdown.
//
// --manifest <file>  Read module paths from a file (one per line).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <vector>
#include <string>

#include "src/bus_driver/bus_driver.h"

#if defined(__Fuchsia__)
#include "src/fuchsia/resource_provider.h"
#endif

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s [--manifest <file>] <module.ko> ...\n", argv[0]);
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

  // Collect module paths from args and manifest files.
  std::vector<std::string> modules;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
      i++;
      FILE* f = fopen(argv[i], "r");
      if (!f) {
        fprintf(stderr, "error: cannot open manifest %s\n", argv[i]);
        continue;
      }
      char line[512];
      while (fgets(line, sizeof(line), f)) {
        // Strip trailing newline.
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
          line[--len] = '\0';
        if (len > 0) modules.emplace_back(line);
      }
      fclose(f);
    } else {
      modules.emplace_back(argv[i]);
    }
  }

  fprintf(stderr, "\n--- Loading %zu module(s) with dependency sorting ---\n",
          modules.size());
  bus.LoadModulesFromFiles(modules);

  fprintf(stderr, "\n--- %zu module(s) loaded ---\n", bus.module_count());

  fprintf(stderr, "Press Enter to unload and exit...\n");
  getchar();

  bus.Shutdown();
  return 0;
}
