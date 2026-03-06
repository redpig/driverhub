// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/dt/dt_provider.h"

#include <cstdio>
#include <cstring>

// Minimal FDT (Flattened Device Tree) parsing for static .dtb blobs.
//
// FDT format (from devicetree specification):
//   - Header: magic, totalsize, structure/strings offsets
//   - Structure block: FDT_BEGIN_NODE / FDT_END_NODE / FDT_PROP tokens
//   - Strings block: null-terminated property name strings
//
// We parse enough to extract:
//   - Node paths and compatible strings
//   - reg properties (MMIO resources)
//   - interrupts properties (IRQ resources)
//   - status property (only "okay" nodes are loaded)

namespace driverhub {

namespace {

// FDT magic and token constants.
constexpr uint32_t FDT_MAGIC = 0xD00DFEED;
constexpr uint32_t FDT_BEGIN_NODE = 0x00000001;
constexpr uint32_t FDT_END_NODE = 0x00000002;
constexpr uint32_t FDT_PROP = 0x00000003;
constexpr uint32_t FDT_END = 0x00000009;

// Read a big-endian 32-bit value from a byte buffer.
uint32_t ReadBe32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) |
         static_cast<uint32_t>(p[3]);
}

// Read a big-endian 64-bit value from a byte buffer.
uint64_t ReadBe64(const uint8_t* p) {
  return (static_cast<uint64_t>(ReadBe32(p)) << 32) | ReadBe32(p + 4);
}

// Align offset to 4-byte boundary.
uint32_t Align4(uint32_t offset) {
  return (offset + 3) & ~3u;
}

// FDT header structure (big-endian).
struct FdtHeader {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
};

FdtHeader ParseHeader(const uint8_t* data) {
  FdtHeader h;
  h.magic = ReadBe32(data + 0);
  h.totalsize = ReadBe32(data + 4);
  h.off_dt_struct = ReadBe32(data + 8);
  h.off_dt_strings = ReadBe32(data + 12);
  h.off_mem_rsvmap = ReadBe32(data + 16);
  h.version = ReadBe32(data + 20);
  h.last_comp_version = ReadBe32(data + 24);
  h.boot_cpuid_phys = ReadBe32(data + 28);
  h.size_dt_strings = ReadBe32(data + 32);
  h.size_dt_struct = ReadBe32(data + 36);
  return h;
}

}  // namespace

StaticDtProvider::StaticDtProvider(const uint8_t* dtb_data, size_t dtb_size)
    : dtb_data_(dtb_data), dtb_size_(dtb_size) {}

std::vector<DtModuleEntry> StaticDtProvider::GetModuleEntries() {
  std::vector<DtModuleEntry> entries;

  if (dtb_size_ < 40) {
    fprintf(stderr, "driverhub: dt: blob too small (%zu bytes)\n", dtb_size_);
    return entries;
  }

  auto header = ParseHeader(dtb_data_);
  if (header.magic != FDT_MAGIC) {
    fprintf(stderr, "driverhub: dt: bad magic 0x%08x (expected 0x%08x)\n",
            header.magic, FDT_MAGIC);
    return entries;
  }

  const uint8_t* struct_base = dtb_data_ + header.off_dt_struct;
  const char* strings_base =
      reinterpret_cast<const char*>(dtb_data_ + header.off_dt_strings);

  uint32_t offset = 0;
  uint32_t struct_size = header.size_dt_struct;

  // Track the current node being built.
  DtModuleEntry current;
  bool in_node = false;
  std::string current_status;
  int depth = 0;
  int target_depth = -1;  // Depth of the node we're currently parsing.

  while (offset < struct_size) {
    uint32_t token = ReadBe32(struct_base + offset);
    offset += 4;

    switch (token) {
      case FDT_BEGIN_NODE: {
        const char* name =
            reinterpret_cast<const char*>(struct_base + offset);
        size_t name_len = strlen(name);
        offset = Align4(offset + name_len + 1);
        depth++;

        // We look for nodes at depth 2+ (children of the root).
        if (depth >= 2 && !in_node) {
          current = DtModuleEntry{};
          current_status.clear();
          in_node = true;
          target_depth = depth;
        }
        break;
      }

      case FDT_END_NODE: {
        if (in_node && depth == target_depth) {
          // Node complete. Add if it has a compatible string and status is
          // "okay" or absent.
          if (!current.compatible.empty() &&
              (current_status.empty() || current_status == "okay")) {
            // Derive module path from compatible string.
            // Convention: "vendor,module-name" → modules/module-name.ko
            std::string mod_name = current.compatible;
            auto comma = mod_name.find(',');
            if (comma != std::string::npos) {
              mod_name = mod_name.substr(comma + 1);
            }
            // Replace hyphens with underscores for module filename.
            for (auto& c : mod_name) {
              if (c == '-') c = '_';
            }
            current.module_path = mod_name + ".ko";

            entries.push_back(std::move(current));
          }
          in_node = false;
          target_depth = -1;
        }
        depth--;
        break;
      }

      case FDT_PROP: {
        uint32_t len = ReadBe32(struct_base + offset);
        uint32_t nameoff = ReadBe32(struct_base + offset + 4);
        offset += 8;

        const char* prop_name = strings_base + nameoff;
        const uint8_t* prop_data = struct_base + offset;
        offset = Align4(offset + len);

        if (!in_node || depth != target_depth) break;

        if (strcmp(prop_name, "compatible") == 0 && len > 0) {
          // First compatible string (null-terminated).
          current.compatible = std::string(
              reinterpret_cast<const char*>(prop_data));
        } else if (strcmp(prop_name, "status") == 0 && len > 0) {
          current_status = std::string(
              reinterpret_cast<const char*>(prop_data));
        } else if (strcmp(prop_name, "reg") == 0) {
          // Parse reg property: pairs of (address, size) in 64-bit cells.
          for (uint32_t i = 0; i + 15 < len; i += 16) {
            DtModuleEntry::Resource res;
            res.type = DtModuleEntry::Resource::kMmio;
            res.base = ReadBe64(prop_data + i);
            res.size = ReadBe64(prop_data + i + 8);
            current.resources.push_back(res);
          }
        } else if (strcmp(prop_name, "interrupts") == 0) {
          // Parse interrupts property: each cell is an IRQ number.
          for (uint32_t i = 0; i + 3 < len; i += 4) {
            DtModuleEntry::Resource res;
            res.type = DtModuleEntry::Resource::kIrq;
            res.base = ReadBe32(prop_data + i);
            res.size = 0;
            current.resources.push_back(res);
          }
        } else {
          // Store other properties as raw bytes.
          current.properties.emplace_back(
              std::string(prop_name),
              std::vector<uint8_t>(prop_data, prop_data + len));
        }
        break;
      }

      case FDT_END:
        goto done;

      default:
        // Skip NOP or unknown tokens.
        break;
    }
  }

done:
  fprintf(stderr, "driverhub: dt: parsed %zu module entries from DTB\n",
          entries.size());
  return entries;
}

VisitorDtProvider::VisitorDtProvider() = default;

std::vector<DtModuleEntry> VisitorDtProvider::GetModuleEntries() {
  // The visitor-based provider receives nodes from the Fuchsia board driver's
  // device tree visitor. This requires the fuchsia.hardware.platform.bus FIDL
  // protocol to be available in the incoming namespace.
  //
  // For now, this returns empty — it will be connected when integrated into
  // a Fuchsia board driver that uses the DT visitor pattern.
  fprintf(stderr, "driverhub: dt: visitor provider not yet connected\n");
  return {};
}

}  // namespace driverhub
