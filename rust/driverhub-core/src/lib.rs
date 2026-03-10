// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! DriverHub core library: ELF loader, modinfo parser, dependency sort,
//! DT parser, and symbol registry — ported from C++ to Rust for memory
//! safety on untrusted input (`.ko` ELF files, DTB blobs).

pub mod allocator;
pub mod dependency_sort;
pub mod dt;
pub mod elf;
pub mod ffi;
pub mod loader;
pub mod modinfo;
pub mod symbols;
