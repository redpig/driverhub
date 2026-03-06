// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for module dependency topological sort.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "src/loader/dependency_sort.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
  if (cond) { printf("  PASS: %s\n", msg); g_pass++; } \
  else { printf("  FAIL: %s\n", msg); g_fail++; } \
} while(0)

int main() {
  printf("\n=== Dependency sort tests ===\n\n");

  // Test 1: No dependencies — should preserve relative order.
  {
    std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
    driverhub::ModuleInfo a, b, c;
    a.name = "a"; b.name = "b"; c.name = "c";
    modules.push_back({"a", a});
    modules.push_back({"b", b});
    modules.push_back({"c", c});

    std::vector<size_t> order;
    bool ok = driverhub::TopologicalSortModules(modules, &order);
    CHECK(ok, "no-deps: sort succeeds");
    CHECK(order.size() == 3, "no-deps: all 3 modules in output");
  }

  // Test 2: Linear dependency chain a -> b -> c.
  {
    std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
    driverhub::ModuleInfo a, b, c;
    a.name = "a";
    b.name = "b"; b.depends = {"a"};
    c.name = "c"; c.depends = {"b"};
    modules.push_back({"a", a});
    modules.push_back({"b", b});
    modules.push_back({"c", c});

    std::vector<size_t> order;
    bool ok = driverhub::TopologicalSortModules(modules, &order);
    CHECK(ok, "linear-chain: sort succeeds");
    CHECK(order.size() == 3, "linear-chain: all 3 modules");

    // a must appear before b, b before c.
    size_t pos_a = 0, pos_b = 0, pos_c = 0;
    for (size_t i = 0; i < order.size(); i++) {
      if (order[i] == 0) pos_a = i;
      if (order[i] == 1) pos_b = i;
      if (order[i] == 2) pos_c = i;
    }
    CHECK(pos_a < pos_b, "linear-chain: a before b");
    CHECK(pos_b < pos_c, "linear-chain: b before c");
  }

  // Test 3: Diamond dependency: d depends on b and c, both depend on a.
  {
    std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
    driverhub::ModuleInfo a, b, c, d;
    a.name = "a";
    b.name = "b"; b.depends = {"a"};
    c.name = "c"; c.depends = {"a"};
    d.name = "d"; d.depends = {"b", "c"};
    modules.push_back({"a", a});
    modules.push_back({"b", b});
    modules.push_back({"c", c});
    modules.push_back({"d", d});

    std::vector<size_t> order;
    bool ok = driverhub::TopologicalSortModules(modules, &order);
    CHECK(ok, "diamond: sort succeeds");
    CHECK(order.size() == 4, "diamond: all 4 modules");

    size_t pos_a = 0, pos_b = 0, pos_c = 0, pos_d = 0;
    for (size_t i = 0; i < order.size(); i++) {
      if (order[i] == 0) pos_a = i;
      if (order[i] == 1) pos_b = i;
      if (order[i] == 2) pos_c = i;
      if (order[i] == 3) pos_d = i;
    }
    CHECK(pos_a < pos_b, "diamond: a before b");
    CHECK(pos_a < pos_c, "diamond: a before c");
    CHECK(pos_b < pos_d, "diamond: b before d");
    CHECK(pos_c < pos_d, "diamond: c before d");
  }

  // Test 4: Dependency cycle — should fail.
  {
    std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
    driverhub::ModuleInfo a, b;
    a.name = "a"; a.depends = {"b"};
    b.name = "b"; b.depends = {"a"};
    modules.push_back({"a", a});
    modules.push_back({"b", b});

    std::vector<size_t> order;
    bool ok = driverhub::TopologicalSortModules(modules, &order);
    CHECK(!ok, "cycle: sort correctly detects cycle");
  }

  // Test 5: External dependency (not in set) — should succeed.
  {
    std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
    driverhub::ModuleInfo a;
    a.name = "a"; a.depends = {"external_lib"};
    modules.push_back({"a", a});

    std::vector<size_t> order;
    bool ok = driverhub::TopologicalSortModules(modules, &order);
    CHECK(ok, "external-dep: sort succeeds (external dep skipped)");
    CHECK(order.size() == 1, "external-dep: module a in output");
  }

  printf("\n--- Results: %d/%d passed ---\n\n",
         g_pass, g_pass + g_fail);
  return g_fail > 0 ? 1 : 0;
}
