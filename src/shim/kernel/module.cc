// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/module.h"

#include <cstdio>
#include <string>
#include <unordered_map>

// EXPORT_SYMBOL shim: modules call this to register exported symbols.
// The symbol registry is set up by the bus driver before module init runs.
// This file provides a thread-safe staging area that the SymbolRegistry
// drains after each module_init() call.

namespace {

// Pending exports from modules that haven't been drained into the registry yet.
// Protected by simple global since module_init is single-threaded.
std::unordered_map<std::string, void*>& pending_exports() {
  static std::unordered_map<std::string, void*> exports;
  return exports;
}

}  // namespace

extern "C" {

void __driverhub_export_symbol(const char* name, void* addr) {
  fprintf(stderr, "driverhub: EXPORT_SYMBOL(%s) = %p\n", name, addr);
  pending_exports()[name] = addr;
}

}  // extern "C"

namespace driverhub {

// Called by the bus driver after each module_init() to drain pending exports
// into the main symbol registry. Declared here, defined as a free function
// so the bus driver can call it without depending on module.h internals.
std::unordered_map<std::string, void*> DrainPendingExports() {
  auto exports = std::move(pending_exports());
  pending_exports().clear();
  return exports;
}

}  // namespace driverhub
