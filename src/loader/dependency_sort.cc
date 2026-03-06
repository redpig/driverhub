// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/dependency_sort.h"

#include <cstdio>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace driverhub {

bool TopologicalSortModules(
    const std::vector<std::pair<std::string, ModuleInfo>>& modules,
    std::vector<size_t>* sorted_indices) {
  sorted_indices->clear();

  // Build name → index map.
  std::unordered_map<std::string, size_t> name_to_idx;
  for (size_t i = 0; i < modules.size(); i++) {
    name_to_idx[modules[i].first] = i;
  }

  // Build adjacency list and in-degree count.
  std::vector<std::vector<size_t>> dependents(modules.size());
  std::vector<int> in_degree(modules.size(), 0);

  for (size_t i = 0; i < modules.size(); i++) {
    for (const auto& dep : modules[i].second.depends) {
      auto it = name_to_idx.find(dep);
      if (it != name_to_idx.end()) {
        // dep must be loaded before modules[i].
        dependents[it->second].push_back(i);
        in_degree[i]++;
      } else {
        // Dependency not in the set — it might already be loaded
        // (e.g., a KMI symbol provider). Skip silently.
        fprintf(stderr,
                "driverhub: dependency '%s' of '%s' not in module set "
                "(may already be loaded)\n",
                dep.c_str(), modules[i].first.c_str());
      }
    }
  }

  // Kahn's algorithm: process nodes with in-degree 0.
  std::queue<size_t> ready;
  for (size_t i = 0; i < modules.size(); i++) {
    if (in_degree[i] == 0) {
      ready.push(i);
    }
  }

  while (!ready.empty()) {
    size_t idx = ready.front();
    ready.pop();
    sorted_indices->push_back(idx);

    for (size_t dep_idx : dependents[idx]) {
      in_degree[dep_idx]--;
      if (in_degree[dep_idx] == 0) {
        ready.push(dep_idx);
      }
    }
  }

  if (sorted_indices->size() != modules.size()) {
    fprintf(stderr, "driverhub: dependency cycle detected! "
                    "Sorted %zu of %zu modules.\n",
            sorted_indices->size(), modules.size());
    return false;
  }

  return true;
}

}  // namespace driverhub
