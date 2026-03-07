// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/modinfo_parser.h"

#include <cstring>
#include <elf.h>
#include <string>

namespace driverhub {

void ParseModinfo(const uint8_t* data, size_t size, ModuleInfo* info) {
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
  if (size < sizeof(Elf64_Ehdr)) {
    return false;
  }

  auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);

  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    return false;
  }

  if (ehdr->e_type != ET_REL || ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
    return false;
  }

  if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) {
    return false;
  }

  auto* shdrs = reinterpret_cast<const Elf64_Shdr*>(data + ehdr->e_shoff);
  uint16_t shnum = ehdr->e_shnum;

  // Section name string table.
  const char* shstrtab = nullptr;
  if (ehdr->e_shstrndx < shnum) {
    shstrtab = reinterpret_cast<const char*>(
        data + shdrs[ehdr->e_shstrndx].sh_offset);
  }

  if (!shstrtab) {
    return false;
  }

  // Find .modinfo section and parse it.
  for (uint16_t i = 0; i < shnum; i++) {
    const char* sec_name = shstrtab + shdrs[i].sh_name;
    if (strcmp(sec_name, ".modinfo") == 0 && shdrs[i].sh_type != SHT_NOBITS) {
      ParseModinfo(data + shdrs[i].sh_offset, shdrs[i].sh_size, info);
      return true;
    }
  }

  // No .modinfo section — still a valid .ko, just no metadata.
  return true;
}

}  // namespace driverhub
