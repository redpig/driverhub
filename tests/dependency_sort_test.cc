// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for module dependency topological sort.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "src/loader/dependency_sort.h"

namespace {

// Helper: find the position of module index `idx` in the sorted order.
size_t FindPosition(const std::vector<size_t>& order, size_t idx) {
  for (size_t i = 0; i < order.size(); i++) {
    if (order[i] == idx) return i;
  }
  ADD_FAILURE() << "index " << idx << " not found in order";
  return order.size();
}

TEST(DependencySortTest, NoDependencies) {
  std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
  driverhub::ModuleInfo a, b, c;
  a.name = "a"; b.name = "b"; c.name = "c";
  modules.push_back({"a", a});
  modules.push_back({"b", b});
  modules.push_back({"c", c});

  std::vector<size_t> order;
  ASSERT_TRUE(driverhub::TopologicalSortModules(modules, &order));
  EXPECT_EQ(order.size(), 3u);
}

TEST(DependencySortTest, LinearChain) {
  std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
  driverhub::ModuleInfo a, b, c;
  a.name = "a";
  b.name = "b"; b.depends = {"a"};
  c.name = "c"; c.depends = {"b"};
  modules.push_back({"a", a});
  modules.push_back({"b", b});
  modules.push_back({"c", c});

  std::vector<size_t> order;
  ASSERT_TRUE(driverhub::TopologicalSortModules(modules, &order));
  ASSERT_EQ(order.size(), 3u);

  // a must appear before b, b before c.
  EXPECT_LT(FindPosition(order, 0), FindPosition(order, 1));
  EXPECT_LT(FindPosition(order, 1), FindPosition(order, 2));
}

TEST(DependencySortTest, DiamondDependency) {
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
  ASSERT_TRUE(driverhub::TopologicalSortModules(modules, &order));
  ASSERT_EQ(order.size(), 4u);

  EXPECT_LT(FindPosition(order, 0), FindPosition(order, 1));  // a before b
  EXPECT_LT(FindPosition(order, 0), FindPosition(order, 2));  // a before c
  EXPECT_LT(FindPosition(order, 1), FindPosition(order, 3));  // b before d
  EXPECT_LT(FindPosition(order, 2), FindPosition(order, 3));  // c before d
}

TEST(DependencySortTest, CycleDetection) {
  std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
  driverhub::ModuleInfo a, b;
  a.name = "a"; a.depends = {"b"};
  b.name = "b"; b.depends = {"a"};
  modules.push_back({"a", a});
  modules.push_back({"b", b});

  std::vector<size_t> order;
  EXPECT_FALSE(driverhub::TopologicalSortModules(modules, &order));
}

TEST(DependencySortTest, ExternalDependencySkipped) {
  std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
  driverhub::ModuleInfo a;
  a.name = "a"; a.depends = {"external_lib"};
  modules.push_back({"a", a});

  std::vector<size_t> order;
  ASSERT_TRUE(driverhub::TopologicalSortModules(modules, &order));
  EXPECT_EQ(order.size(), 1u);
}

TEST(DependencySortTest, EmptyModuleList) {
  std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;

  std::vector<size_t> order;
  ASSERT_TRUE(driverhub::TopologicalSortModules(modules, &order));
  EXPECT_EQ(order.size(), 0u);
}

TEST(DependencySortTest, SelfDependency) {
  std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
  driverhub::ModuleInfo a;
  a.name = "a"; a.depends = {"a"};
  modules.push_back({"a", a});

  std::vector<size_t> order;
  EXPECT_FALSE(driverhub::TopologicalSortModules(modules, &order));
}

TEST(DependencySortTest, LongChain) {
  // Build a chain of 10 modules: m0 <- m1 <- m2 <- ... <- m9.
  std::vector<std::pair<std::string, driverhub::ModuleInfo>> modules;
  for (int i = 0; i < 10; i++) {
    driverhub::ModuleInfo m;
    m.name = "m" + std::to_string(i);
    if (i > 0) m.depends = {"m" + std::to_string(i - 1)};
    modules.push_back({m.name, m});
  }

  std::vector<size_t> order;
  ASSERT_TRUE(driverhub::TopologicalSortModules(modules, &order));
  ASSERT_EQ(order.size(), 10u);

  // Verify strict ordering: each module appears after its dependency.
  for (int i = 1; i < 10; i++) {
    EXPECT_LT(FindPosition(order, i - 1), FindPosition(order, i))
        << "m" << (i - 1) << " should appear before m" << i;
  }
}

}  // namespace
