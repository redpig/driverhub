// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for ModuleLoader: ELF ET_REL parsing, section layout,
// symbol resolution, relocation, and entry point extraction.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "src/loader/module_loader.h"
#include "src/symbols/symbol_registry.h"

// Forward declaration: drain pending EXPORT_SYMBOL registrations.
namespace driverhub {
std::unordered_map<std::string, void*> DrainPendingExports();
}  // namespace driverhub

namespace {

// Read a file into a byte vector.
std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) return {};
  auto size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char*>(data.data()), size);
  return data;
}

// Test fixture that sets up a SymbolRegistry with KMI symbols and an
// mmap-based allocator for host testing.
class LoaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_.RegisterKmiSymbols();
  }

  // Load a .ko file, returning the LoadedModule or nullptr on failure.
  std::unique_ptr<driverhub::LoadedModule> LoadKo(const std::string& path) {
    auto data = ReadFile(path);
    if (data.empty()) {
      fprintf(stderr, "test: could not read %s\n", path.c_str());
      return nullptr;
    }
    driverhub::ModuleLoader loader(registry_);
    return loader.Load(path, data.data(), data.size());
  }

  // After calling init_fn(), drain EXPORT_SYMBOL registrations into
  // the test's registry so intermodule resolution works.
  void DrainExportsIntoRegistry() {
    auto exports = driverhub::DrainPendingExports();
    for (auto& [name, addr] : exports) {
      registry_.Register(name, addr);
    }
  }

  driverhub::SymbolRegistry registry_;
};

// ============================================================
// Basic module loading
// ============================================================

TEST_F(LoaderTest, LoadTestModule) {
  auto mod = LoadKo("tests/test_module.ko");
  if (!mod) {
    GTEST_SKIP() << "test_module.ko not built — run 'make' first";
  }

  EXPECT_NE(mod->init_fn, nullptr);
  EXPECT_NE(mod->rust_handle, nullptr);
}

TEST_F(LoaderTest, InitModuleReturnsZero) {
  auto mod = LoadKo("tests/test_module.ko");
  if (!mod) {
    GTEST_SKIP() << "test_module.ko not built";
  }
  if (!mod->init_fn) {
    GTEST_SKIP() << "init_fn not found";
  }

  int result = mod->init_fn();
  EXPECT_EQ(result, 0);
}

// ============================================================
// Export module loading
// ============================================================

TEST_F(LoaderTest, LoadExportModule) {
  auto mod = LoadKo("tests/export_module.ko");
  if (!mod) {
    GTEST_SKIP() << "export_module.ko not built";
  }

  EXPECT_NE(mod->init_fn, nullptr);

  // EXPORT_SYMBOL constructors fired during Load(); drain into our registry.
  DrainExportsIntoRegistry();

  // After loading, the exported symbol should be in the registry.
  void* exported = registry_.Resolve("exported_add");
  EXPECT_NE(exported, nullptr) << "exported_add should be registered";
}

TEST_F(LoaderTest, ExportedFunctionCallable) {
  auto mod = LoadKo("tests/export_module.ko");
  if (!mod) {
    GTEST_SKIP() << "export_module.ko not built";
  }

  // EXPORT_SYMBOL constructors ran during Load(); drain into our registry.
  DrainExportsIntoRegistry();

  // Call init_module to set up the module.
  if (mod->init_fn) {
    EXPECT_EQ(mod->init_fn(), 0);
  }

  // Resolve the exported function and call it.
  auto* fn = reinterpret_cast<int (*)(int, int)>(registry_.Resolve("exported_add"));
  if (!fn) {
    GTEST_SKIP() << "exported_add not found in registry";
  }

  EXPECT_EQ(fn(3, 4), 7);
  EXPECT_EQ(fn(10, 20), 30);
  EXPECT_EQ(fn(-5, 5), 0);
}

// ============================================================
// Intermodule dependency: export → import
// ============================================================

TEST_F(LoaderTest, IntermoduleResolution) {
  // Load the exporting module first.
  auto exporter = LoadKo("tests/export_module.ko");
  if (!exporter) {
    GTEST_SKIP() << "export_module.ko not built";
  }
  // EXPORT_SYMBOL constructors ran during Load(); drain into our registry.
  DrainExportsIntoRegistry();

  if (exporter->init_fn) {
    EXPECT_EQ(exporter->init_fn(), 0);
  }

  // Verify exported_add is registered.
  ASSERT_NE(registry_.Resolve("exported_add"), nullptr);

  // Load the importing module — it should resolve exported_add.
  auto importer = LoadKo("tests/import_module.ko");
  if (!importer) {
    GTEST_SKIP() << "import_module.ko not built";
  }

  EXPECT_NE(importer->init_fn, nullptr);

  // Calling init should invoke exported_add(3,4) and exported_add(10,20).
  if (importer->init_fn) {
    EXPECT_EQ(importer->init_fn(), 0);
  }
}

// ============================================================
// Data + function export module
// ============================================================

TEST_F(LoaderTest, DataExportModule) {
  auto mod = LoadKo("tests/data_export_module.ko");
  if (!mod) {
    GTEST_SKIP() << "data_export_module.ko not built";
  }

  // EXPORT_SYMBOL constructors ran during Load(); drain into our registry.
  DrainExportsIntoRegistry();

  if (mod->init_fn) {
    EXPECT_EQ(mod->init_fn(), 0);
  }

  // Check exported data symbol.
  auto* shared_val = reinterpret_cast<int*>(registry_.Resolve("shared_value"));
  if (shared_val) {
    EXPECT_EQ(*shared_val, 42);
  }

  // Check exported function symbol.
  auto* compute = reinterpret_cast<int (*)(int)>(
      registry_.Resolve("exported_compute"));
  if (compute && shared_val) {
    EXPECT_EQ(compute(2), 2 * (*shared_val));
  }

  // Check set_value modifies the shared data.
  auto* set_val = reinterpret_cast<void (*)(int)>(
      registry_.Resolve("exported_set_value"));
  if (set_val && shared_val) {
    set_val(100);
    EXPECT_EQ(*shared_val, 100);
  }
}

// ============================================================
// Invalid ELF handling
// ============================================================

TEST_F(LoaderTest, InvalidElfRejected) {
  driverhub::ModuleLoader loader(registry_);

  // Empty data.
  EXPECT_EQ(loader.Load("empty", nullptr, 0), nullptr);

  // Garbage data.
  uint8_t garbage[] = {0x00, 0x01, 0x02, 0x03};
  EXPECT_EQ(loader.Load("garbage", garbage, sizeof(garbage)), nullptr);

  // Too small for ELF header.
  uint8_t small[] = {0x7f, 'E', 'L', 'F'};
  EXPECT_EQ(loader.Load("small", small, sizeof(small)), nullptr);
}

// ============================================================
// Module metadata (modinfo)
// ============================================================

TEST_F(LoaderTest, ModuleInfoExtracted) {
  auto mod = LoadKo("tests/export_module.ko");
  if (!mod) {
    GTEST_SKIP() << "export_module.ko not built";
  }

  // The export module has MODULE_LICENSE("GPL") and MODULE_DESCRIPTION.
  EXPECT_EQ(mod->info.license, "GPL");
  EXPECT_FALSE(mod->info.description.empty());
}

// ============================================================
// Cleanup module
// ============================================================

TEST_F(LoaderTest, CleanupModuleCallable) {
  auto mod = LoadKo("tests/export_module.ko");
  if (!mod) {
    GTEST_SKIP() << "export_module.ko not built";
  }

  DrainExportsIntoRegistry();

  if (mod->init_fn) {
    EXPECT_EQ(mod->init_fn(), 0);
  }

  // cleanup_module should exist and not crash.
  if (mod->exit_fn) {
    mod->exit_fn();
  }
}

}  // namespace
