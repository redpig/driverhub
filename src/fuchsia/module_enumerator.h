// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_MODULE_ENUMERATOR_H_
#define DRIVERHUB_SRC_FUCHSIA_MODULE_ENUMERATOR_H_

#include <string>
#include <vector>

namespace driverhub {

// Describes a module discovered in the package data directory.
struct ModuleEntry {
  std::string name;          // Module name (without .ko extension).
  std::string path;          // Full path within the package namespace.
  std::vector<uint8_t> data; // Raw ELF bytes (populated after read).
};

// Enumerates .ko modules from the driver package's data directory.
//
// On Fuchsia, modules are bundled as package resources under /pkg/data/modules/.
// The component manifest routes this directory to the driver's namespace.
//
// The enumerator:
//   1. Opens the modules directory from the incoming namespace.
//   2. Lists all .ko files.
//   3. Reads each into a buffer for the loader.
//
// For host testing, a filesystem path can be used instead.
class ModuleEnumerator {
 public:
  // Enumerate modules from a directory path (host or Fuchsia namespace).
  static std::vector<ModuleEntry> EnumerateFromDirectory(
      const std::string& dir_path);

  // Enumerate modules from a list of explicit file paths.
  static std::vector<ModuleEntry> EnumerateFromPaths(
      const std::vector<std::string>& paths);
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_FUCHSIA_MODULE_ENUMERATOR_H_
