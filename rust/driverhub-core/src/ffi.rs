// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! C FFI bridge for calling Rust code from C++.
//!
//! Exports opaque-handle-based APIs that map to the Rust types.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;

use crate::dependency_sort;
use crate::loader::{LoadedModule, ModuleLoader};
use crate::modinfo::{self, ModuleInfo};
use crate::symbols::SymbolRegistry;

// ============================================================
// ModuleInfo FFI
// ============================================================

/// C-compatible module info structure.
/// Strings are owned by this struct and freed in `dh_module_info_free`.
#[repr(C)]
pub struct DhModuleInfo {
    pub name: *mut c_char,
    pub description: *mut c_char,
    pub license: *mut c_char,
    pub vermagic: *mut c_char,
    pub depends: *mut *mut c_char,
    pub depends_count: usize,
    pub aliases: *mut *mut c_char,
    pub aliases_count: usize,
}

impl DhModuleInfo {
    fn from_rust(info: &ModuleInfo) -> Self {
        let depends: Vec<*mut c_char> = info
            .depends
            .iter()
            .map(|s| CString::new(s.as_str()).unwrap().into_raw())
            .collect();
        let aliases: Vec<*mut c_char> = info
            .aliases
            .iter()
            .map(|s| CString::new(s.as_str()).unwrap().into_raw())
            .collect();

        let depends_count = depends.len();
        let aliases_count = aliases.len();
        let depends_ptr = if depends.is_empty() {
            ptr::null_mut()
        } else {
            let ptr = depends.as_ptr() as *mut *mut c_char;
            std::mem::forget(depends);
            ptr
        };
        let aliases_ptr = if aliases.is_empty() {
            ptr::null_mut()
        } else {
            let ptr = aliases.as_ptr() as *mut *mut c_char;
            std::mem::forget(aliases);
            ptr
        };

        DhModuleInfo {
            name: CString::new(info.name.as_str()).unwrap().into_raw(),
            description: CString::new(info.description.as_str())
                .unwrap()
                .into_raw(),
            license: CString::new(info.license.as_str()).unwrap().into_raw(),
            vermagic: CString::new(info.vermagic.as_str()).unwrap().into_raw(),
            depends: depends_ptr,
            depends_count,
            aliases: aliases_ptr,
            aliases_count,
        }
    }
}

/// Free a `DhModuleInfo` and all its owned strings.
#[no_mangle]
pub unsafe extern "C" fn dh_module_info_free(info: *mut DhModuleInfo) {
    if info.is_null() {
        return;
    }
    let info = &mut *info;
    if !info.name.is_null() {
        drop(CString::from_raw(info.name));
    }
    if !info.description.is_null() {
        drop(CString::from_raw(info.description));
    }
    if !info.license.is_null() {
        drop(CString::from_raw(info.license));
    }
    if !info.vermagic.is_null() {
        drop(CString::from_raw(info.vermagic));
    }
    if !info.depends.is_null() {
        let deps = Vec::from_raw_parts(info.depends, info.depends_count, info.depends_count);
        for d in deps {
            if !d.is_null() {
                drop(CString::from_raw(d));
            }
        }
    }
    if !info.aliases.is_null() {
        let aliases =
            Vec::from_raw_parts(info.aliases, info.aliases_count, info.aliases_count);
        for a in aliases {
            if !a.is_null() {
                drop(CString::from_raw(a));
            }
        }
    }
}

/// Extract module info from a .ko ELF file. Returns true on success.
#[no_mangle]
pub unsafe extern "C" fn dh_extract_module_info(
    data: *const u8,
    size: usize,
    out: *mut DhModuleInfo,
) -> bool {
    if data.is_null() || out.is_null() || size == 0 {
        return false;
    }
    let slice = std::slice::from_raw_parts(data, size);
    let mut info = ModuleInfo::default();
    if modinfo::extract_module_info(slice, &mut info) {
        *out = DhModuleInfo::from_rust(&info);
        true
    } else {
        false
    }
}

// ============================================================
// SymbolRegistry FFI
// ============================================================

#[no_mangle]
pub extern "C" fn dh_symbol_registry_new() -> *mut SymbolRegistry {
    Box::into_raw(Box::new(SymbolRegistry::new()))
}

#[no_mangle]
pub unsafe extern "C" fn dh_symbol_registry_free(reg: *mut SymbolRegistry) {
    if !reg.is_null() {
        drop(Box::from_raw(reg));
    }
}

#[no_mangle]
pub unsafe extern "C" fn dh_symbol_registry_register(
    reg: *mut SymbolRegistry,
    name: *const c_char,
    addr: *mut std::ffi::c_void,
) {
    if reg.is_null() || name.is_null() {
        return;
    }
    let name = CStr::from_ptr(name).to_str().unwrap_or("");
    (*reg).register(name, addr);
}

#[no_mangle]
pub unsafe extern "C" fn dh_symbol_registry_resolve(
    reg: *const SymbolRegistry,
    name: *const c_char,
) -> *mut std::ffi::c_void {
    if reg.is_null() || name.is_null() {
        return ptr::null_mut();
    }
    let name = CStr::from_ptr(name).to_str().unwrap_or("");
    (*reg).resolve(name)
}

#[no_mangle]
pub unsafe extern "C" fn dh_symbol_registry_size(reg: *const SymbolRegistry) -> usize {
    if reg.is_null() {
        0
    } else {
        (*reg).len()
    }
}

#[no_mangle]
pub unsafe extern "C" fn dh_symbol_registry_contains(
    reg: *const SymbolRegistry,
    name: *const c_char,
) -> bool {
    if reg.is_null() || name.is_null() {
        return false;
    }
    let name = CStr::from_ptr(name).to_str().unwrap_or("");
    (*reg).contains(name)
}

// ============================================================
// Dependency Sort FFI
// ============================================================

/// Topological sort of modules. Returns true on success.
///
/// `names`: array of `count` C strings (module names)
/// `depends_lists`: array of `count` arrays of C strings (dependency names)
/// `depends_counts`: array of `count` sizes for each depends list
/// `sorted_indices`: output array of `count` indices in load order
#[no_mangle]
pub unsafe extern "C" fn dh_topological_sort(
    names: *const *const c_char,
    depends_lists: *const *const *const c_char,
    depends_counts: *const usize,
    count: usize,
    sorted_indices: *mut usize,
) -> bool {
    if names.is_null() || sorted_indices.is_null() || count == 0 {
        return true; // Empty is valid.
    }

    let mut modules: Vec<(String, ModuleInfo)> = Vec::with_capacity(count);
    for i in 0..count {
        let name = CStr::from_ptr(*names.add(i))
            .to_str()
            .unwrap_or("")
            .to_string();
        let mut info = ModuleInfo {
            name: name.clone(),
            ..Default::default()
        };

        if !depends_lists.is_null() && !depends_counts.is_null() {
            let dep_count = *depends_counts.add(i);
            let dep_list = *depends_lists.add(i);
            if !dep_list.is_null() {
                for j in 0..dep_count {
                    let dep = CStr::from_ptr(*dep_list.add(j))
                        .to_str()
                        .unwrap_or("")
                        .to_string();
                    info.depends.push(dep);
                }
            }
        }

        modules.push((name, info));
    }

    match dependency_sort::topological_sort_modules(&modules) {
        Some(order) => {
            for (i, &idx) in order.iter().enumerate() {
                *sorted_indices.add(i) = idx;
            }
            true
        }
        None => false,
    }
}

// ============================================================
// Module Loader FFI
// ============================================================

/// Opaque loader handle wrapping a `SymbolRegistry` pointer and optional
/// external allocator callbacks.
pub struct DhModuleLoader {
    registry: *mut SymbolRegistry,
    alloc_fn: Option<crate::allocator::AllocFn>,
    dealloc_fn: Option<crate::allocator::DeallocFn>,
}

#[no_mangle]
pub unsafe extern "C" fn dh_module_loader_new(
    registry: *mut SymbolRegistry,
) -> *mut DhModuleLoader {
    if registry.is_null() {
        return ptr::null_mut();
    }
    Box::into_raw(Box::new(DhModuleLoader {
        registry,
        alloc_fn: None,
        dealloc_fn: None,
    }))
}

/// Create a module loader with an external allocator (e.g. VMO-backed on Fuchsia).
#[no_mangle]
pub unsafe extern "C" fn dh_module_loader_new_with_allocator(
    registry: *mut SymbolRegistry,
    alloc_fn: crate::allocator::AllocFn,
    dealloc_fn: crate::allocator::DeallocFn,
) -> *mut DhModuleLoader {
    if registry.is_null() {
        return ptr::null_mut();
    }
    Box::into_raw(Box::new(DhModuleLoader {
        registry,
        alloc_fn: Some(alloc_fn),
        dealloc_fn: Some(dealloc_fn),
    }))
}

#[no_mangle]
pub unsafe extern "C" fn dh_module_loader_free(loader: *mut DhModuleLoader) {
    if !loader.is_null() {
        drop(Box::from_raw(loader));
    }
}

#[no_mangle]
pub unsafe extern "C" fn dh_module_loader_load(
    loader: *mut DhModuleLoader,
    name: *const c_char,
    data: *const u8,
    size: usize,
) -> *mut LoadedModule {
    if loader.is_null() || name.is_null() || data.is_null() || size == 0 {
        return ptr::null_mut();
    }
    let name_str = CStr::from_ptr(name).to_str().unwrap_or("unknown");
    let data_slice = std::slice::from_raw_parts(data, size);
    let registry = &mut *(*loader).registry;
    let mut ml = if let (Some(alloc_fn), Some(dealloc_fn)) =
        ((*loader).alloc_fn, (*loader).dealloc_fn)
    {
        ModuleLoader::with_allocator(registry, alloc_fn, dealloc_fn)
    } else {
        ModuleLoader::new(registry)
    };
    match ml.load(name_str, data_slice) {
        Some(module) => Box::into_raw(Box::new(module)),
        None => ptr::null_mut(),
    }
}

// ============================================================
// LoadedModule FFI
// ============================================================

#[no_mangle]
pub unsafe extern "C" fn dh_loaded_module_free(module: *mut LoadedModule) {
    if !module.is_null() {
        drop(Box::from_raw(module));
    }
}

#[no_mangle]
pub unsafe extern "C" fn dh_loaded_module_info(
    module: *const LoadedModule,
) -> *mut DhModuleInfo {
    if module.is_null() {
        return ptr::null_mut();
    }
    let info = DhModuleInfo::from_rust(&(*module).info);
    Box::into_raw(Box::new(info))
}

/// Returns the init function pointer, or NULL if none.
#[no_mangle]
pub unsafe extern "C" fn dh_loaded_module_init_fn(
    module: *const LoadedModule,
) -> Option<unsafe extern "C" fn() -> i32> {
    if module.is_null() {
        None
    } else {
        (*module).init_fn
    }
}

/// Returns the exit function pointer, or NULL if none.
#[no_mangle]
pub unsafe extern "C" fn dh_loaded_module_exit_fn(
    module: *const LoadedModule,
) -> Option<unsafe extern "C" fn()> {
    if module.is_null() {
        None
    } else {
        (*module).exit_fn
    }
}

// ============================================================
// DT Parser FFI
// ============================================================

/// C-compatible DT module entry.
#[repr(C)]
pub struct DhDtEntry {
    pub compatible: *mut c_char,
    pub module_path: *mut c_char,
    pub resource_count: usize,
}

/// Parse a static DTB blob. Returns the number of entries written.
#[no_mangle]
pub unsafe extern "C" fn dh_dt_parse_static(
    dtb: *const u8,
    dtb_size: usize,
    entries: *mut DhDtEntry,
    max_entries: usize,
) -> usize {
    if dtb.is_null() || dtb_size == 0 || entries.is_null() || max_entries == 0 {
        return 0;
    }
    let dtb_slice = std::slice::from_raw_parts(dtb, dtb_size);
    let parsed = crate::dt::parse_static_dtb(dtb_slice);
    let count = parsed.len().min(max_entries);
    for (i, entry) in parsed.iter().take(count).enumerate() {
        let out = &mut *entries.add(i);
        out.compatible = CString::new(entry.compatible.as_str())
            .unwrap()
            .into_raw();
        out.module_path = CString::new(entry.module_path.as_str())
            .unwrap()
            .into_raw();
        out.resource_count = entry.resources.len();
    }
    count
}

/// Free a DhDtEntry's owned strings.
#[no_mangle]
pub unsafe extern "C" fn dh_dt_entry_free(entry: *mut DhDtEntry) {
    if entry.is_null() {
        return;
    }
    let e = &mut *entry;
    if !e.compatible.is_null() {
        drop(CString::from_raw(e.compatible));
    }
    if !e.module_path.is_null() {
        drop(CString::from_raw(e.module_path));
    }
}
