// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Symbol registry for KMI shim and intermodule symbol resolution.
//!
//! A simple name-to-address map. The C++ shim layer registers KMI symbols
//! at startup, and the loader registers intermodule EXPORT_SYMBOL symbols
//! as modules are loaded.

use std::collections::HashMap;

/// Registry mapping symbol names to their addresses.
#[derive(Debug)]
pub struct SymbolRegistry {
    symbols: HashMap<String, *mut std::ffi::c_void>,
}

// SymbolRegistry stores raw pointers that are Send across threads — the
// addresses come from the shim library and loaded modules, both of which
// live for the process lifetime.
unsafe impl Send for SymbolRegistry {}
unsafe impl Sync for SymbolRegistry {}

impl SymbolRegistry {
    /// Create an empty registry.
    pub fn new() -> Self {
        Self {
            symbols: HashMap::new(),
        }
    }

    /// Register a symbol. If the name already exists, it is overwritten.
    pub fn register(&mut self, name: &str, addr: *mut std::ffi::c_void) {
        self.symbols.insert(name.to_string(), addr);
    }

    /// Resolve a symbol by name. Returns null if not found.
    pub fn resolve(&self, name: &str) -> *mut std::ffi::c_void {
        self.symbols
            .get(name)
            .copied()
            .unwrap_or(std::ptr::null_mut())
    }

    /// Check if a symbol is registered.
    pub fn contains(&self, name: &str) -> bool {
        self.symbols.contains_key(name)
    }

    /// Number of registered symbols.
    pub fn len(&self) -> usize {
        self.symbols.len()
    }

    /// Whether the registry is empty.
    pub fn is_empty(&self) -> bool {
        self.symbols.is_empty()
    }
}

impl Default for SymbolRegistry {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn register_and_resolve() {
        let mut reg = SymbolRegistry::new();
        let val: u64 = 0x1234;
        let addr = &val as *const u64 as *mut std::ffi::c_void;
        reg.register("kmalloc", addr);

        assert_eq!(reg.resolve("kmalloc"), addr);
        assert!(reg.resolve("kfree").is_null());
    }

    #[test]
    fn contains_and_size() {
        let mut reg = SymbolRegistry::new();
        assert!(reg.is_empty());
        assert!(!reg.contains("foo"));

        reg.register("foo", std::ptr::null_mut());
        assert!(reg.contains("foo"));
        assert_eq!(reg.len(), 1);
    }

    #[test]
    fn overwrite() {
        let mut reg = SymbolRegistry::new();
        let a: u64 = 1;
        let b: u64 = 2;
        reg.register("sym", &a as *const u64 as *mut std::ffi::c_void);
        reg.register("sym", &b as *const u64 as *mut std::ffi::c_void);
        assert_eq!(reg.resolve("sym"), &b as *const u64 as *mut std::ffi::c_void);
        assert_eq!(reg.len(), 1);
    }
}
