// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the SymbolRegistry: registration, resolution, intermodule
// dependencies, and duplicate handling.

#include <string>

#include <gtest/gtest.h>

#include "src/symbols/symbol_registry.h"

namespace {

// Helper: register a symbol and verify it resolves.
void RegisterAndResolve(driverhub::SymbolRegistry& reg,
                        const std::string& name, void* addr) {
  reg.Register(name, addr);
  ASSERT_TRUE(reg.Contains(name));
  EXPECT_EQ(reg.Resolve(name), addr);
}

// ============================================================
// Basic registration and resolution
// ============================================================

TEST(SymbolRegistryTest, RegisterAndResolve) {
  driverhub::SymbolRegistry reg;
  int dummy = 42;
  RegisterAndResolve(reg, "test_symbol", &dummy);
  EXPECT_EQ(reg.size(), 1u);
}

TEST(SymbolRegistryTest, ResolveUnregisteredReturnsNull) {
  driverhub::SymbolRegistry reg;
  EXPECT_EQ(reg.Resolve("nonexistent"), nullptr);
  EXPECT_FALSE(reg.Contains("nonexistent"));
}

TEST(SymbolRegistryTest, MultipleSymbols) {
  driverhub::SymbolRegistry reg;
  int a = 1, b = 2, c = 3;
  reg.Register("a", &a);
  reg.Register("b", &b);
  reg.Register("c", &c);
  EXPECT_EQ(reg.size(), 3u);
  EXPECT_EQ(reg.Resolve("a"), &a);
  EXPECT_EQ(reg.Resolve("b"), &b);
  EXPECT_EQ(reg.Resolve("c"), &c);
}

// ============================================================
// Duplicate / overwrite handling
// ============================================================

TEST(SymbolRegistryTest, DuplicateOverwrites) {
  driverhub::SymbolRegistry reg;
  int first = 1, second = 2;
  reg.Register("sym", &first);
  EXPECT_EQ(reg.Resolve("sym"), &first);

  // Second registration overwrites.
  reg.Register("sym", &second);
  EXPECT_EQ(reg.Resolve("sym"), &second);
  EXPECT_EQ(reg.size(), 1u);
}

// ============================================================
// KMI symbol bulk registration
// ============================================================

TEST(SymbolRegistryTest, RegisterKmiSymbolsNotEmpty) {
  driverhub::SymbolRegistry reg;
  reg.RegisterKmiSymbols();
  // KMI symbols include kmalloc, printk, mutex_lock, etc.
  EXPECT_GT(reg.size(), 100u);
}

TEST(SymbolRegistryTest, KmiSymbolsResolvable) {
  driverhub::SymbolRegistry reg;
  reg.RegisterKmiSymbols();

  // Core kernel API symbols that every .ko needs.
  EXPECT_NE(reg.Resolve("kmalloc"), nullptr);
  EXPECT_NE(reg.Resolve("kfree"), nullptr);
  EXPECT_NE(reg.Resolve("printk"), nullptr);
  EXPECT_NE(reg.Resolve("mutex_lock"), nullptr);
  EXPECT_NE(reg.Resolve("mutex_unlock"), nullptr);
}

// ============================================================
// Intermodule symbol pattern: EXPORT_SYMBOL flow
// ============================================================

TEST(SymbolRegistryTest, IntermoduleExportImport) {
  driverhub::SymbolRegistry reg;
  reg.RegisterKmiSymbols();

  // Simulate module A exporting a function.
  int exported_fn = 0;
  reg.Register("exported_add", &exported_fn);

  // Module B should resolve it.
  EXPECT_EQ(reg.Resolve("exported_add"), &exported_fn);
}

TEST(SymbolRegistryTest, IntermoduleDataAndFunction) {
  driverhub::SymbolRegistry reg;

  // Simulate data_export_module.c exports.
  int shared_value = 42;
  int compute_fn = 0;
  int set_value_fn = 0;

  reg.Register("shared_value", &shared_value);
  reg.Register("exported_compute", &compute_fn);
  reg.Register("exported_set_value", &set_value_fn);

  // Simulate data_import_module.c resolving.
  EXPECT_EQ(reg.Resolve("shared_value"), &shared_value);
  EXPECT_EQ(reg.Resolve("exported_compute"), &compute_fn);
  EXPECT_EQ(reg.Resolve("exported_set_value"), &set_value_fn);
}

// ============================================================
// Empty name and edge cases
// ============================================================

TEST(SymbolRegistryTest, EmptyNameRegistration) {
  driverhub::SymbolRegistry reg;
  int val = 0;
  reg.Register("", &val);
  EXPECT_EQ(reg.Resolve(""), &val);
}

TEST(SymbolRegistryTest, NullAddressRegistration) {
  driverhub::SymbolRegistry reg;
  reg.Register("null_sym", nullptr);
  EXPECT_TRUE(reg.Contains("null_sym"));
  EXPECT_EQ(reg.Resolve("null_sym"), nullptr);
}

}  // namespace
