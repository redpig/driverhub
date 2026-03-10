// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_LOADER_MODINFO_PARSER_H_
#define DRIVERHUB_SRC_LOADER_MODINFO_PARSER_H_

#include <cstddef>
#include <cstdint>

#include "src/loader/module_loader.h"

namespace driverhub {

// Extract module metadata from a .ko ELF file without fully loading it.
// Delegates to the Rust dh_extract_module_info() FFI function.
//
// Returns true on success, false if the file is not a valid .ko.
bool ExtractModuleInfo(const uint8_t* data, size_t size, ModuleInfo* info);

// Parse null-terminated key=value pairs from raw .modinfo section data.
void ParseModinfo(const uint8_t* data, size_t size, ModuleInfo* info);

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_LOADER_MODINFO_PARSER_H_
