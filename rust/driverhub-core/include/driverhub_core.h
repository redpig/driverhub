// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// C FFI header for the Rust driverhub-core library.
//
// Link against libdriverhub_core.a (built with `cargo build --release`).

#ifndef DRIVERHUB_RUST_DRIVERHUB_CORE_H_
#define DRIVERHUB_RUST_DRIVERHUB_CORE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Module Info
// ============================================================

typedef struct {
    char* name;
    char* description;
    char* license;
    char* vermagic;
    char** depends;
    size_t depends_count;
    char** aliases;
    size_t aliases_count;
} DhModuleInfo;

// Extract module info from a .ko ELF file. Returns true on success.
bool dh_extract_module_info(const uint8_t* data, size_t size,
                             DhModuleInfo* out);

// Free a DhModuleInfo and all its owned strings.
void dh_module_info_free(DhModuleInfo* info);

// ============================================================
// Symbol Registry
// ============================================================

typedef struct DhSymbolRegistry DhSymbolRegistry;

DhSymbolRegistry* dh_symbol_registry_new(void);
void dh_symbol_registry_free(DhSymbolRegistry* reg);
void dh_symbol_registry_register(DhSymbolRegistry* reg, const char* name,
                                  void* addr);
void* dh_symbol_registry_resolve(const DhSymbolRegistry* reg,
                                  const char* name);
size_t dh_symbol_registry_size(const DhSymbolRegistry* reg);
bool dh_symbol_registry_contains(const DhSymbolRegistry* reg,
                                  const char* name);

// ============================================================
// Dependency Sort
// ============================================================

// Topological sort of modules. Returns true on success (no cycles).
// `names`: array of `count` C strings (module names).
// `depends_lists`: array of `count` arrays of dependency name C strings.
// `depends_counts`: array of `count` sizes for each depends array.
// `sorted_indices`: output array of `count` indices in load order.
bool dh_topological_sort(const char** names,
                          const char* const* const* depends_lists,
                          const size_t* depends_counts, size_t count,
                          size_t* sorted_indices);

// ============================================================
// Module Loader
// ============================================================

typedef struct DhModuleLoader DhModuleLoader;
typedef struct DhLoadedModule DhLoadedModule;

// Allocator callback types for platform-specific memory allocation.
// alloc_fn: allocate `size` bytes of RWX memory, return pointer or NULL.
// dealloc_fn: free memory previously returned by alloc_fn.
typedef uint8_t* (*dh_alloc_fn_t)(size_t size);
typedef void (*dh_dealloc_fn_t)(uint8_t* ptr, size_t size);

DhModuleLoader* dh_module_loader_new(DhSymbolRegistry* registry);

// Create a module loader with an external allocator (e.g. VMO-backed).
DhModuleLoader* dh_module_loader_new_with_allocator(DhSymbolRegistry* registry,
                                                     dh_alloc_fn_t alloc_fn,
                                                     dh_dealloc_fn_t dealloc_fn);
void dh_module_loader_free(DhModuleLoader* loader);

DhLoadedModule* dh_module_loader_load(DhModuleLoader* loader,
                                       const char* name,
                                       const uint8_t* data, size_t size);

void dh_loaded_module_free(DhLoadedModule* module);
DhModuleInfo* dh_loaded_module_info(const DhLoadedModule* module);

typedef int (*dh_init_fn_t)(void);
typedef void (*dh_exit_fn_t)(void);

dh_init_fn_t dh_loaded_module_init_fn(const DhLoadedModule* module);
dh_exit_fn_t dh_loaded_module_exit_fn(const DhLoadedModule* module);

// ============================================================
// Device Tree Parser
// ============================================================

typedef struct {
    char* compatible;
    char* module_path;
    size_t resource_count;
} DhDtEntry;

// Parse a static DTB blob. Returns the number of entries written.
size_t dh_dt_parse_static(const uint8_t* dtb, size_t dtb_size,
                           DhDtEntry* entries, size_t max_entries);

// Free a DhDtEntry's owned strings.
void dh_dt_entry_free(DhDtEntry* entry);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_RUST_DRIVERHUB_CORE_H_
