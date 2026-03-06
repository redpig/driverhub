// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_LOADER_DEPENDENCY_SORT_H_
#define DRIVERHUB_SRC_LOADER_DEPENDENCY_SORT_H_

#include <string>
#include <vector>

#include "src/loader/module_loader.h"

namespace driverhub {

// Topologically sort modules by their dependency relationships.
//
// Reads the `depends` field from each module's ModuleInfo to build a
// dependency graph, then produces a load order where each module appears
// after all of its dependencies.
//
// Returns true on success, false if a dependency cycle is detected.
// On success, `sorted_indices` contains indices into the input vector
// in the correct load order.
bool TopologicalSortModules(
    const std::vector<std::pair<std::string, ModuleInfo>>& modules,
    std::vector<size_t>* sorted_indices);

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_LOADER_DEPENDENCY_SORT_H_
