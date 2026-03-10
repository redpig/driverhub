// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/dependency_sort.h"

#include <cstdio>
#include <vector>

#include "driverhub_core.h"

namespace driverhub {

bool TopologicalSortModules(
    const std::vector<std::pair<std::string, ModuleInfo>>& modules,
    std::vector<size_t>* sorted_indices) {
  sorted_indices->clear();

  size_t count = modules.size();
  if (count == 0) return true;

  // Build C arrays for the Rust FFI.
  std::vector<const char*> names(count);
  std::vector<std::vector<const char*>> dep_ptrs(count);
  std::vector<const char* const*> depends_lists(count);
  std::vector<size_t> depends_counts(count);

  for (size_t i = 0; i < count; i++) {
    names[i] = modules[i].first.c_str();
    dep_ptrs[i].reserve(modules[i].second.depends.size());
    for (const auto& dep : modules[i].second.depends) {
      dep_ptrs[i].push_back(dep.c_str());
    }
    depends_lists[i] = dep_ptrs[i].empty() ? nullptr : dep_ptrs[i].data();
    depends_counts[i] = dep_ptrs[i].size();
  }

  sorted_indices->resize(count);

  bool ok = dh_topological_sort(
      names.data(),
      reinterpret_cast<const char* const* const*>(depends_lists.data()),
      depends_counts.data(),
      count,
      sorted_indices->data());

  if (!ok) {
    fprintf(stderr, "driverhub: dependency cycle detected!\n");
    sorted_indices->clear();
    return false;
  }

  return true;
}

}  // namespace driverhub
