// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/modinfo_parser.h"

#include <cstring>
#include <string>
#include <string_view>

#include "driverhub_core.h"

namespace driverhub {

void ParseModinfo(const uint8_t* data, size_t size, ModuleInfo* info) {
  if (!data || size == 0) return;
  const char* p = reinterpret_cast<const char*>(data);
  const char* end = p + size;

  while (p < end) {
    size_t len = strnlen(p, end - p);
    if (len == 0) {
      p++;
      continue;
    }

    std::string_view entry(p, len);
    auto eq = entry.find('=');
    if (eq != std::string_view::npos) {
      auto key = entry.substr(0, eq);
      auto val = entry.substr(eq + 1);

      if (key == "description") {
        info->description = std::string(val);
      } else if (key == "author") {
        info->author = std::string(val);
      } else if (key == "license") {
        info->license = std::string(val);
      } else if (key == "vermagic") {
        info->vermagic = std::string(val);
      } else if (key == "depends") {
        std::string deps(val);
        size_t pos = 0;
        while (pos < deps.size()) {
          auto comma = deps.find(',', pos);
          if (comma == std::string::npos) comma = deps.size();
          auto dep = deps.substr(pos, comma - pos);
          if (!dep.empty()) {
            info->depends.push_back(dep);
          }
          pos = comma + 1;
        }
      } else if (key == "alias") {
        info->aliases.push_back(std::string(val));
      }
    }

    p += len + 1;
  }
}

bool ExtractModuleInfo(const uint8_t* data, size_t size, ModuleInfo* info) {
  DhModuleInfo dh_info = {};
  if (!dh_extract_module_info(data, size, &dh_info)) {
    return false;
  }

  if (dh_info.name) info->name = dh_info.name;
  if (dh_info.description) info->description = dh_info.description;
  if (dh_info.license) info->license = dh_info.license;
  if (dh_info.vermagic) info->vermagic = dh_info.vermagic;
  for (size_t i = 0; i < dh_info.depends_count; i++) {
    if (dh_info.depends[i]) {
      info->depends.push_back(dh_info.depends[i]);
    }
  }
  for (size_t i = 0; i < dh_info.aliases_count; i++) {
    if (dh_info.aliases[i]) {
      info->aliases.push_back(dh_info.aliases[i]);
    }
  }

  dh_module_info_free(&dh_info);
  return true;
}

}  // namespace driverhub
