// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuchsia/module_enumerator.h"

#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <algorithm>

namespace driverhub {

std::vector<ModuleEntry> ModuleEnumerator::EnumerateFromDirectory(
    const std::string& dir_path) {
  std::vector<ModuleEntry> entries;

  DIR* dir = opendir(dir_path.c_str());
  if (!dir) {
    fprintf(stderr, "driverhub: cannot open module directory: %s\n",
            dir_path.c_str());
    return entries;
  }

  struct dirent* ent;
  while ((ent = readdir(dir)) != nullptr) {
    std::string filename = ent->d_name;
    // Only process .ko files.
    if (filename.size() < 4 ||
        filename.substr(filename.size() - 3) != ".ko") {
      continue;
    }

    std::string full_path = dir_path + "/" + filename;
    std::string name = filename.substr(0, filename.size() - 3);

    // Read the file contents.
    std::ifstream file(full_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      fprintf(stderr, "driverhub: cannot open module: %s\n",
              full_path.c_str());
      continue;
    }

    size_t size = file.tellg();
    file.seekg(0);

    ModuleEntry entry;
    entry.name = name;
    entry.path = full_path;
    entry.data.resize(size);
    file.read(reinterpret_cast<char*>(entry.data.data()), size);

    fprintf(stderr, "driverhub: enumerated module: %s (%zu bytes)\n",
            name.c_str(), size);
    entries.push_back(std::move(entry));
  }

  closedir(dir);

  // Sort by name for deterministic ordering.
  std::sort(entries.begin(), entries.end(),
            [](const ModuleEntry& a, const ModuleEntry& b) {
              return a.name < b.name;
            });

  return entries;
}

std::vector<ModuleEntry> ModuleEnumerator::EnumerateFromPaths(
    const std::vector<std::string>& paths) {
  std::vector<ModuleEntry> entries;

  for (const auto& path : paths) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      fprintf(stderr, "driverhub: cannot open module: %s\n", path.c_str());
      continue;
    }

    size_t size = file.tellg();
    file.seekg(0);

    // Derive name from path.
    std::string name = path;
    auto slash = name.rfind('/');
    if (slash != std::string::npos) {
      name = name.substr(slash + 1);
    }
    if (name.size() > 3 && name.substr(name.size() - 3) == ".ko") {
      name = name.substr(0, name.size() - 3);
    }

    ModuleEntry entry;
    entry.name = name;
    entry.path = path;
    entry.data.resize(size);
    file.read(reinterpret_cast<char*>(entry.data.data()), size);

    entries.push_back(std::move(entry));
  }

  return entries;
}

}  // namespace driverhub
