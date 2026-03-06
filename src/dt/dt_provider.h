// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_DT_DT_PROVIDER_H_
#define DRIVERHUB_SRC_DT_DT_PROVIDER_H_

// Device tree integration for module discovery and configuration.
//
// Supports two modes:
//   - Static: Parse a bundled .dtb blob at startup.
//   - Dynamic: Use Fuchsia's device tree visitor to receive nodes from the
//     board driver at runtime.
//
// Each DT node with a recognized `compatible` string maps to a .ko module
// to be loaded, with DT properties providing configuration to the module's
// probe function.

#include <cstdint>
#include <string>
#include <vector>

namespace driverhub {

// A device tree node describing a module to load.
struct DtModuleEntry {
  std::string compatible;                       // DT compatible string
  std::string module_path;                      // Path to .ko file
  std::vector<std::pair<std::string, std::vector<uint8_t>>> properties;

  // Resources extracted from DT.
  struct Resource {
    enum Type { kMmio, kIrq };
    Type type;
    uint64_t base;
    uint64_t size;  // For MMIO; IRQ number for kIrq.
  };
  std::vector<Resource> resources;
};

// Abstract interface for device tree providers.
class DtProvider {
 public:
  virtual ~DtProvider() = default;

  // Enumerate all module entries from the device tree.
  virtual std::vector<DtModuleEntry> GetModuleEntries() = 0;
};

// Static DT provider: parses a .dtb blob bundled as a component resource.
class StaticDtProvider : public DtProvider {
 public:
  explicit StaticDtProvider(const uint8_t* dtb_data, size_t dtb_size);
  std::vector<DtModuleEntry> GetModuleEntries() override;

 private:
  const uint8_t* dtb_data_;
  size_t dtb_size_;
};

// Dynamic DT provider: receives nodes from Fuchsia's device tree visitor.
class VisitorDtProvider : public DtProvider {
 public:
  VisitorDtProvider();
  std::vector<DtModuleEntry> GetModuleEntries() override;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_DT_DT_PROVIDER_H_
