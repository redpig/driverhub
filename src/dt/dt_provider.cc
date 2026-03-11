// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/dt/dt_provider.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

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

// ---------------------------------------------------------------------------
// VisitorDtProvider implementation
// ---------------------------------------------------------------------------

namespace {

// Minimal JSON-ish helpers.  The manifest schema is small and predictable so
// we hand-parse rather than pulling in a full JSON library.

// Skip whitespace in |s| starting at |pos|.
void SkipWs(const std::string& s, size_t& pos) {
  while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                            s[pos] == '\n' || s[pos] == '\r')) {
    ++pos;
  }
}

// Expect a specific character, return false on mismatch.
bool Expect(const std::string& s, size_t& pos, char ch) {
  SkipWs(s, pos);
  if (pos >= s.size() || s[pos] != ch) return false;
  ++pos;
  return true;
}

// Parse a JSON string value (opening '"' already expected next).
bool ParseString(const std::string& s, size_t& pos, std::string& out) {
  SkipWs(s, pos);
  if (pos >= s.size() || s[pos] != '"') return false;
  ++pos;
  out.clear();
  while (pos < s.size() && s[pos] != '"') {
    if (s[pos] == '\\' && pos + 1 < s.size()) {
      ++pos;
    }
    out += s[pos++];
  }
  if (pos >= s.size()) return false;
  ++pos;  // closing '"'
  return true;
}

// Parse a JSON integer (positive, decimal).
bool ParseUint64(const std::string& s, size_t& pos, uint64_t& out) {
  SkipWs(s, pos);
  if (pos >= s.size()) return false;

  // Support hex "0x..." inside quoted strings and bare integers.
  bool quoted = false;
  if (s[pos] == '"') {
    quoted = true;
    ++pos;
  }

  if (pos >= s.size()) return false;

  char* end = nullptr;
  out = strtoull(s.c_str() + pos, &end, 0);
  if (end == s.c_str() + pos) return false;
  pos = end - s.c_str();

  if (quoted) {
    if (pos >= s.size() || s[pos] != '"') return false;
    ++pos;
  }
  return true;
}

// Parse an array of uint64 values: [ 32, 33, ... ]
bool ParseUint64Array(const std::string& s, size_t& pos,
                      std::vector<uint64_t>& out) {
  if (!Expect(s, pos, '[')) return false;
  SkipWs(s, pos);
  while (pos < s.size() && s[pos] != ']') {
    uint64_t val;
    if (!ParseUint64(s, pos, val)) return false;
    out.push_back(val);
    SkipWs(s, pos);
    if (pos < s.size() && s[pos] == ',') ++pos;
  }
  return Expect(s, pos, ']');
}

// Parse a reg array: [ { "base": "0x...", "size": "0x..." }, ... ]
bool ParseRegArray(const std::string& s, size_t& pos,
                   std::vector<DtModuleEntry::Resource>& out) {
  if (!Expect(s, pos, '[')) return false;
  SkipWs(s, pos);
  while (pos < s.size() && s[pos] != ']') {
    if (!Expect(s, pos, '{')) return false;
    DtModuleEntry::Resource res;
    res.type = DtModuleEntry::Resource::kMmio;
    res.base = 0;
    res.size = 0;

    // Parse key/value pairs inside the reg object.
    SkipWs(s, pos);
    while (pos < s.size() && s[pos] != '}') {
      std::string key;
      if (!ParseString(s, pos, key)) return false;
      if (!Expect(s, pos, ':')) return false;
      uint64_t val;
      if (!ParseUint64(s, pos, val)) return false;
      if (key == "base") {
        res.base = val;
      } else if (key == "size") {
        res.size = val;
      }
      SkipWs(s, pos);
      if (pos < s.size() && s[pos] == ',') ++pos;
      SkipWs(s, pos);
    }
    if (!Expect(s, pos, '}')) return false;
    out.push_back(res);
    SkipWs(s, pos);
    if (pos < s.size() && s[pos] == ',') ++pos;
    SkipWs(s, pos);
  }
  return Expect(s, pos, ']');
}

// Parse a flat properties object: { "key": "value", ... }
// Values are stored as raw UTF-8 bytes.
bool ParsePropertiesObject(
    const std::string& s, size_t& pos,
    std::vector<std::pair<std::string, std::vector<uint8_t>>>& out) {
  if (!Expect(s, pos, '{')) return false;
  SkipWs(s, pos);
  while (pos < s.size() && s[pos] != '}') {
    std::string key;
    if (!ParseString(s, pos, key)) return false;
    if (!Expect(s, pos, ':')) return false;
    std::string value;
    if (!ParseString(s, pos, value)) return false;
    out.emplace_back(key,
                     std::vector<uint8_t>(value.begin(), value.end()));
    SkipWs(s, pos);
    if (pos < s.size() && s[pos] == ',') ++pos;
    SkipWs(s, pos);
  }
  return Expect(s, pos, '}');
}

}  // namespace

// ---------------------------------------------------------------------------

static constexpr const char* kDefaultManifestPath =
    "/pkg/data/dt-visitor.json";

VisitorDtProvider::VisitorDtProvider()
    : manifest_path_(kDefaultManifestPath) {}

VisitorDtProvider::VisitorDtProvider(std::string manifest_path)
    : manifest_path_(std::move(manifest_path)) {}

// static
std::string VisitorDtProvider::ModulePathFromCompatible(
    const std::string& compatible) {
  std::string mod_name = compatible;
  auto comma = mod_name.find(',');
  if (comma != std::string::npos) {
    mod_name = mod_name.substr(comma + 1);
  }
  for (auto& c : mod_name) {
    if (c == '-') c = '_';
  }
  return mod_name + ".ko";
}

bool VisitorDtProvider::TryVisitorService(
    std::vector<DtModuleEntry>& entries) {
  // In a real Fuchsia integration, this method would:
  //
  //   1. Open the fuchsia.hardware.platform.bus/PlatformBus protocol from the
  //      incoming namespace.
  //   2. Call GetBoardInfo() to get the board name and revision.
  //   3. Use the device tree visitor API to walk the board's DT:
  //        - For each node with a recognised "compatible" string, create a
  //          DtModuleEntry with MMIO/IRQ resources from "reg" / "interrupts".
  //        - Expose each node as a NodeProperty with the compatible string
  //          for DFv2 bind rule matching.
  //   4. Return true if the walk succeeds, false if the protocol is
  //      unavailable or the board doesn't expose a DT.
  //
  // For now, the visitor service is not yet available in all board
  // configurations, so we log what we would do and return false to trigger
  // the manifest fallback path.

  fprintf(stderr,
          "driverhub: dt: visitor: attempting connection to DT visitor "
          "service...\n");
  fprintf(stderr,
          "driverhub: dt: visitor: service not available in current "
          "configuration, falling back to manifest\n");
  return false;
}

bool VisitorDtProvider::ParseManifest(std::vector<DtModuleEntry>& entries) {
  // Read the manifest file.
  std::ifstream file(manifest_path_);
  if (!file.is_open()) {
    fprintf(stderr,
            "driverhub: dt: visitor: cannot open manifest '%s': %s\n",
            manifest_path_.c_str(), strerror(errno));
    return false;
  }

  std::ostringstream ss;
  ss << file.rdbuf();
  std::string content = ss.str();
  file.close();

  fprintf(stderr,
          "driverhub: dt: visitor: parsing manifest '%s' (%zu bytes)\n",
          manifest_path_.c_str(), content.size());

  // Top-level: { "nodes": [ ... ] }
  size_t pos = 0;
  if (!Expect(content, pos, '{')) {
    fprintf(stderr, "driverhub: dt: visitor: expected '{' at start\n");
    return false;
  }

  std::string top_key;
  if (!ParseString(content, pos, top_key) || top_key != "nodes") {
    fprintf(stderr,
            "driverhub: dt: visitor: expected \"nodes\" key, got \"%s\"\n",
            top_key.c_str());
    return false;
  }
  if (!Expect(content, pos, ':')) {
    fprintf(stderr, "driverhub: dt: visitor: expected ':' after \"nodes\"\n");
    return false;
  }
  if (!Expect(content, pos, '[')) {
    fprintf(stderr, "driverhub: dt: visitor: expected '[' for nodes array\n");
    return false;
  }

  // Parse each node object.
  SkipWs(content, pos);
  while (pos < content.size() && content[pos] != ']') {
    if (!Expect(content, pos, '{')) {
      fprintf(stderr, "driverhub: dt: visitor: expected '{' for node\n");
      return false;
    }

    DtModuleEntry entry;
    std::string status;

    // Parse key/value pairs within this node.
    SkipWs(content, pos);
    while (pos < content.size() && content[pos] != '}') {
      std::string key;
      if (!ParseString(content, pos, key)) {
        fprintf(stderr,
                "driverhub: dt: visitor: failed to parse key at pos %zu\n",
                pos);
        return false;
      }
      if (!Expect(content, pos, ':')) {
        fprintf(stderr,
                "driverhub: dt: visitor: expected ':' after key \"%s\"\n",
                key.c_str());
        return false;
      }

      if (key == "compatible") {
        if (!ParseString(content, pos, entry.compatible)) return false;
      } else if (key == "status") {
        if (!ParseString(content, pos, status)) return false;
      } else if (key == "module_path") {
        if (!ParseString(content, pos, entry.module_path)) return false;
      } else if (key == "reg") {
        if (!ParseRegArray(content, pos, entry.resources)) return false;
      } else if (key == "interrupts") {
        std::vector<uint64_t> irqs;
        if (!ParseUint64Array(content, pos, irqs)) return false;
        for (auto irq : irqs) {
          DtModuleEntry::Resource res;
          res.type = DtModuleEntry::Resource::kIrq;
          res.base = irq;
          res.size = 0;
          entry.resources.push_back(res);
        }
      } else if (key == "properties") {
        if (!ParsePropertiesObject(content, pos, entry.properties))
          return false;
      } else {
        // Skip unknown values — try string, then number.
        std::string dummy_str;
        uint64_t dummy_uint;
        if (pos < content.size() && content[pos] == '"') {
          ParseString(content, pos, dummy_str);
        } else if (pos < content.size() && content[pos] == '[') {
          // Skip unknown array.
          int depth_count = 1;
          ++pos;
          while (pos < content.size() && depth_count > 0) {
            if (content[pos] == '[') ++depth_count;
            if (content[pos] == ']') --depth_count;
            ++pos;
          }
        } else if (pos < content.size() && content[pos] == '{') {
          // Skip unknown object.
          int depth_count = 1;
          ++pos;
          while (pos < content.size() && depth_count > 0) {
            if (content[pos] == '{') ++depth_count;
            if (content[pos] == '}') --depth_count;
            ++pos;
          }
        } else {
          ParseUint64(content, pos, dummy_uint);
        }
      }

      SkipWs(content, pos);
      if (pos < content.size() && content[pos] == ',') ++pos;
      SkipWs(content, pos);
    }

    if (!Expect(content, pos, '}')) {
      fprintf(stderr, "driverhub: dt: visitor: expected '}' for node\n");
      return false;
    }

    // Only include nodes with status "okay" or no status field.
    if (!status.empty() && status != "okay") {
      fprintf(stderr,
              "driverhub: dt: visitor: skipping node '%s' (status='%s')\n",
              entry.compatible.c_str(), status.c_str());
    } else if (entry.compatible.empty()) {
      fprintf(stderr,
              "driverhub: dt: visitor: skipping node without compatible "
              "string\n");
    } else {
      // Derive module path from compatible string if not explicitly set.
      if (entry.module_path.empty()) {
        entry.module_path = ModulePathFromCompatible(entry.compatible);
      }
      fprintf(stderr,
              "driverhub: dt: visitor: node '%s' -> module '%s' "
              "(resources=%zu, properties=%zu)\n",
              entry.compatible.c_str(), entry.module_path.c_str(),
              entry.resources.size(), entry.properties.size());
      entries.push_back(std::move(entry));
    }

    SkipWs(content, pos);
    if (pos < content.size() && content[pos] == ',') ++pos;
    SkipWs(content, pos);
  }

  // Closing ] and }
  if (!Expect(content, pos, ']') || !Expect(content, pos, '}')) {
    fprintf(stderr, "driverhub: dt: visitor: malformed manifest tail\n");
    return false;
  }

  return true;
}

std::vector<DtModuleEntry> VisitorDtProvider::GetModuleEntries() {
  std::vector<DtModuleEntry> entries;

  // First, try the live Fuchsia DT visitor service.
  if (TryVisitorService(entries)) {
    fprintf(stderr,
            "driverhub: dt: visitor: enumerated %zu entries from DT "
            "visitor service\n",
            entries.size());
    return entries;
  }

  // Fallback: parse the JSON manifest.
  if (ParseManifest(entries)) {
    fprintf(stderr,
            "driverhub: dt: visitor: enumerated %zu entries from manifest "
            "'%s'\n",
            entries.size(), manifest_path_.c_str());
    return entries;
  }

  // Neither source available — return empty, the bus driver should handle
  // the absence of DT entries gracefully.
  fprintf(stderr,
          "driverhub: dt: visitor: no DT entries available (service "
          "unavailable, no manifest)\n");
  return entries;
}

}  // namespace driverhub
