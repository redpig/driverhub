// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! ELF ET_REL loader for Linux .ko kernel modules.
//!
//! Parses sections, allocates executable memory, resolves symbols against
//! the KMI shim registry, applies architecture-specific relocations
//! (AArch64 and x86_64), patches out kCFI checks and privileged
//! instructions, and extracts init/exit entry points.

use std::collections::HashMap;

use crate::allocator::{
    external_allocate, mmap_allocate, AllocFn, DeallocFn, MemoryAllocation,
};
use crate::elf;
use crate::modinfo::{self, ModuleInfo};
use crate::symbols::SymbolRegistry;

/// A loaded .ko module in memory.
pub struct LoadedModule {
    pub name: String,
    pub info: ModuleInfo,
    pub init_fn: Option<unsafe extern "C" fn() -> i32>,
    pub exit_fn: Option<unsafe extern "C" fn()>,
    pub exports: HashMap<String, *mut std::ffi::c_void>,
    pub allocation: Option<MemoryAllocation>,
}

// LoadedModule stores raw fn pointers and an mmap allocation.
unsafe impl Send for LoadedModule {}

impl LoadedModule {
    /// Look up an exported symbol by name.
    pub fn resolve(&self, name: &str) -> *mut std::ffi::c_void {
        self.exports
            .get(name)
            .copied()
            .unwrap_or(std::ptr::null_mut())
    }
}

/// Loads .ko modules, resolving symbols against a shared registry.
pub struct ModuleLoader<'a> {
    symbols: &'a mut SymbolRegistry,
    alloc_fn: Option<AllocFn>,
    dealloc_fn: Option<DeallocFn>,
}

/// Internal: a section loaded into memory.
struct LoadedSection {
    base: *mut u8,
    size: usize,
    name: String,
}

impl<'a> ModuleLoader<'a> {
    pub fn new(symbols: &'a mut SymbolRegistry) -> Self {
        Self {
            symbols,
            alloc_fn: None,
            dealloc_fn: None,
        }
    }

    /// Create a loader with an external allocator callback.
    pub fn with_allocator(
        symbols: &'a mut SymbolRegistry,
        alloc_fn: AllocFn,
        dealloc_fn: DeallocFn,
    ) -> Self {
        Self {
            symbols,
            alloc_fn: Some(alloc_fn),
            dealloc_fn: Some(dealloc_fn),
        }
    }

    /// Allocate memory using the external allocator if set, else mmap.
    fn allocate(&self, size: usize) -> Option<MemoryAllocation> {
        if let (Some(alloc_fn), Some(dealloc_fn)) = (self.alloc_fn, self.dealloc_fn) {
            external_allocate(size, alloc_fn, dealloc_fn)
        } else {
            mmap_allocate(size)
        }
    }

    /// Load a .ko module from raw ELF bytes.
    pub fn load(&mut self, name: &str, data: &[u8]) -> Option<LoadedModule> {
        let ehdr = elf::parse_ehdr(data).or_else(|| {
            eprintln!("driverhub: {}: too small or bad ELF magic", name);
            None
        })?;

        if ehdr.e_type != elf::ET_REL {
            eprintln!("driverhub: {}: not ET_REL (type={})", name, ehdr.e_type);
            return None;
        }
        if ehdr.ei_class != elf::ELFCLASS64 {
            eprintln!("driverhub: {}: not 64-bit ELF", name);
            return None;
        }

        // Architecture check.
        let expected_machine = if cfg!(target_arch = "aarch64") {
            elf::EM_AARCH64
        } else if cfg!(target_arch = "x86_64") {
            elf::EM_X86_64
        } else {
            0
        };
        if expected_machine != 0 && ehdr.e_machine != expected_machine {
            eprintln!(
                "driverhub: {}: wrong architecture (ELF machine={}, expected={})",
                name, ehdr.e_machine, expected_machine
            );
            return None;
        }

        if ehdr.e_shoff == 0 || ehdr.e_shnum == 0 {
            eprintln!("driverhub: {}: no section headers", name);
            return None;
        }

        let shdrs = elf::parse_shdrs(data, &ehdr)?;
        let shnum = shdrs.len();

        // Get section name string table.
        let shstrtab = if (ehdr.e_shstrndx as usize) < shnum {
            let s = &shdrs[ehdr.e_shstrndx as usize];
            let start = s.sh_offset as usize;
            let end = start + s.sh_size as usize;
            if end <= data.len() {
                Some(&data[start..end])
            } else {
                None
            }
        } else {
            None
        };

        // First pass: calculate total allocation size and catalog sections.
        let mut section_names: Vec<String> = Vec::with_capacity(shnum);
        let mut total_alloc: usize = 0;

        for shdr in &shdrs {
            let sec_name = shstrtab
                .map(|st| elf::read_str(st, shdr.sh_name as usize).to_string())
                .unwrap_or_default();
            section_names.push(sec_name);

            if shdr.sh_flags & elf::SHF_ALLOC != 0 {
                let align = if shdr.sh_addralign > 0 {
                    shdr.sh_addralign as usize
                } else {
                    1
                };
                total_alloc = (total_alloc + align - 1) & !(align - 1);
                total_alloc += shdr.sh_size as usize;
            }
        }

        // Count relocations that may need trampolines or GOT entries.
        let mut trampoline_count: usize = 0;
        let mut got_entry_count: usize = 0;
        for shdr in &shdrs {
            if shdr.sh_type != elf::SHT_RELA {
                continue;
            }
            let relas = elf::parse_relas(data, shdr);
            for rela in &relas {
                let rtype = rela.r_type();
                if cfg!(target_arch = "aarch64") {
                    if rtype == elf::R_AARCH64_CALL26 || rtype == elf::R_AARCH64_JUMP26 {
                        trampoline_count += 1;
                    }
                    if rtype == elf::R_AARCH64_ADR_PREL_PG_HI21 {
                        got_entry_count += 1;
                    }
                } else if cfg!(target_arch = "x86_64") {
                    if rtype == elf::R_X86_64_PC32 || rtype == elf::R_X86_64_PLT32 {
                        trampoline_count += 1;
                    }
                }
            }
        }

        let trampoline_size: usize = if cfg!(target_arch = "aarch64") { 16 } else { 14 };
        let got_entry_size: usize = 8;

        total_alloc = (total_alloc + 15) & !15;
        let mut trampoline_offset = total_alloc;
        total_alloc += trampoline_count * trampoline_size;
        total_alloc = (total_alloc + 7) & !7;
        let mut got_offset = total_alloc;
        total_alloc += got_entry_count * got_entry_size;

        // Allocate RWX memory (external allocator if set, else mmap).
        let mut alloc = self.allocate(total_alloc).or_else(|| {
            eprintln!(
                "driverhub: {}: memory allocation failed for {} bytes",
                name, total_alloc
            );
            None
        })?;
        let alloc_base = alloc.base;

        // Second pass: copy section data into allocation.
        let mut sections: Vec<LoadedSection> = Vec::with_capacity(shnum);
        let mut offset: usize = 0;
        for (i, shdr) in shdrs.iter().enumerate() {
            if shdr.sh_flags & elf::SHF_ALLOC == 0 {
                sections.push(LoadedSection {
                    base: std::ptr::null_mut(),
                    size: 0,
                    name: section_names[i].clone(),
                });
                continue;
            }

            let align = if shdr.sh_addralign > 0 {
                shdr.sh_addralign as usize
            } else {
                1
            };
            offset = (offset + align - 1) & !(align - 1);
            let sec_base = unsafe { alloc_base.add(offset) };

            if shdr.sh_type != elf::SHT_NOBITS {
                let src_start = shdr.sh_offset as usize;
                let src_end = src_start + shdr.sh_size as usize;
                if src_end <= data.len() {
                    unsafe {
                        std::ptr::copy_nonoverlapping(
                            data[src_start..src_end].as_ptr(),
                            sec_base,
                            shdr.sh_size as usize,
                        );
                    }
                }
            }
            // SHT_NOBITS (.bss) — already zero from mmap.

            sections.push(LoadedSection {
                base: sec_base,
                size: shdr.sh_size as usize,
                name: section_names[i].clone(),
            });
            offset += shdr.sh_size as usize;
        }

        // Find symbol table and string table.
        let mut symtab_idx = None;
        for (i, shdr) in shdrs.iter().enumerate() {
            if shdr.sh_type == elf::SHT_SYMTAB {
                symtab_idx = Some(i);
                break;
            }
        }
        let symtab_shdr = match symtab_idx {
            Some(i) => &shdrs[i],
            None => {
                eprintln!("driverhub: {}: no symbol table found", name);
                alloc.release();
                return None;
            }
        };
        let syms = elf::parse_symtab(data, symtab_shdr);

        let strtab_shdr = &shdrs[symtab_shdr.sh_link as usize];
        let strtab_start = strtab_shdr.sh_offset as usize;
        let strtab_end = strtab_start + strtab_shdr.sh_size as usize;
        let strtab = if strtab_end <= data.len() {
            &data[strtab_start..strtab_end]
        } else {
            eprintln!("driverhub: {}: string table out of bounds", name);
            alloc.release();
            return None;
        };

        // Resolve symbol addresses.
        let mut sym_addrs: Vec<*mut std::ffi::c_void> =
            vec![std::ptr::null_mut(); syms.len()];

        for (i, sym) in syms.iter().enumerate() {
            let sym_name = elf::read_str(strtab, sym.st_name as usize);

            if sym.st_shndx == elf::SHN_UNDEF {
                if sym.st_name == 0 {
                    continue;
                }
                let addr = self.symbols.resolve(sym_name);
                if addr.is_null() {
                    eprintln!("driverhub: {}: unresolved symbol: {}", name, sym_name);
                }
                sym_addrs[i] = addr;
            } else if (sym.st_shndx as usize) < shnum {
                let sec = &sections[sym.st_shndx as usize];
                if !sec.base.is_null() {
                    sym_addrs[i] =
                        unsafe { sec.base.add(sym.st_value as usize) as *mut std::ffi::c_void };
                }
            } else if sym.st_shndx == elf::SHN_ABS {
                sym_addrs[i] = sym.st_value as *mut std::ffi::c_void;
            }
        }

        // Apply relocations.
        for shdr in &shdrs {
            if shdr.sh_type != elf::SHT_RELA {
                continue;
            }
            let target_sec = shdr.sh_info as usize;
            if target_sec >= shnum || sections[target_sec].base.is_null() {
                continue;
            }

            let relas = elf::parse_relas(data, shdr);
            for rela in &relas {
                let sym_idx = rela.r_sym() as usize;
                let rtype = rela.r_type();
                let patch_addr =
                    unsafe { sections[target_sec].base.add(rela.r_offset as usize) };
                let sym_val = if sym_idx < sym_addrs.len() {
                    sym_addrs[sym_idx]
                } else {
                    std::ptr::null_mut()
                };
                let s = sym_val as u64;
                let a = rela.r_addend as u64;
                let p = patch_addr as u64;

                unsafe {
                    self.apply_relocation(
                        name,
                        rtype,
                        patch_addr,
                        s,
                        a,
                        p,
                        alloc_base,
                        &mut alloc,
                        &mut trampoline_offset,
                        trampoline_size,
                        &mut got_offset,
                        got_entry_size,
                        sym_idx as u32,
                    );
                }
            }
        }

        // Patch kCFI checks (AArch64: brk #0x8200..#0x82FF → NOP).
        if cfg!(target_arch = "aarch64") {
            Self::patch_kcfi(&shdrs, &sections);
        }

        // Patch privileged instructions (AArch64: MSR/MRS for EL1 sysregs → NOP/MOV).
        if cfg!(target_arch = "aarch64") {
            Self::patch_privileged(&shdrs, &sections);
        }

        // Run .init_array constructors (EXPORT_SYMBOL registration).
        for sec in &sections {
            if sec.name == ".init_array" && !sec.base.is_null() {
                let count = sec.size / std::mem::size_of::<fn()>();
                let ctors =
                    unsafe { std::slice::from_raw_parts(sec.base as *const Option<fn()>, count) };
                for ctor in ctors {
                    if let Some(f) = ctor {
                        f();
                    }
                }
            }
        }

        // Build result.
        let mut module = LoadedModule {
            name: name.to_string(),
            info: ModuleInfo::default(),
            init_fn: None,
            exit_fn: None,
            exports: HashMap::new(),
            allocation: None,
        };

        // Extract .modinfo metadata.
        for (i, sec) in sections.iter().enumerate() {
            if sec.name == ".modinfo" && shdrs[i].sh_type != elf::SHT_NOBITS {
                let start = shdrs[i].sh_offset as usize;
                let end = start + shdrs[i].sh_size as usize;
                if end <= data.len() {
                    modinfo::parse_modinfo(&data[start..end], &mut module.info);
                }
                break;
            }
        }
        module.info.name = name.to_string();

        // Find init_module, cleanup_module, and all GLOBAL exports.
        for (i, sym) in syms.iter().enumerate() {
            let sym_name = elf::read_str(strtab, sym.st_name as usize);
            if sym_name == "init_module" {
                module.init_fn = Some(unsafe {
                    std::mem::transmute::<*mut std::ffi::c_void, unsafe extern "C" fn() -> i32>(
                        sym_addrs[i],
                    )
                });
            } else if sym_name == "cleanup_module" {
                module.exit_fn = Some(unsafe {
                    std::mem::transmute::<*mut std::ffi::c_void, unsafe extern "C" fn()>(
                        sym_addrs[i],
                    )
                });
            }

            if !sym_addrs[i].is_null()
                && sym.st_bind() == elf::STB_GLOBAL
                && sym.st_shndx != elf::SHN_UNDEF
                && sym.st_name != 0
            {
                module
                    .exports
                    .insert(sym_name.to_string(), sym_addrs[i]);
            }
        }

        // Register exports in the shared symbol registry.
        for (sym_name, addr) in &module.exports {
            self.symbols.register(sym_name, *addr);
        }

        module.allocation = Some(alloc);

        eprintln!(
            "driverhub: loaded {} ({} bytes, {} symbols, init={}, exit={})",
            name,
            data.len(),
            syms.len(),
            module.init_fn.is_some(),
            module.exit_fn.is_some(),
        );

        Some(module)
    }

    /// Apply a single relocation. Handles both AArch64 and x86_64.
    #[allow(clippy::too_many_arguments)]
    unsafe fn apply_relocation(
        &self,
        name: &str,
        rtype: u32,
        patch_addr: *mut u8,
        s: u64,
        a: u64,
        p: u64,
        alloc_base: *mut u8,
        alloc: &mut MemoryAllocation,
        trampoline_offset: &mut usize,
        trampoline_size: usize,
        got_offset: &mut usize,
        got_entry_size: usize,
        sym_idx: u32,
    ) {
        if cfg!(target_arch = "aarch64") {
            self.apply_aarch64_relocation(
                name,
                rtype,
                patch_addr,
                s,
                a,
                p,
                alloc_base,
                alloc,
                trampoline_offset,
                trampoline_size,
                got_offset,
                got_entry_size,
                sym_idx,
            );
        } else if cfg!(target_arch = "x86_64") {
            self.apply_x86_64_relocation(
                name,
                rtype,
                patch_addr,
                s,
                a,
                p,
                alloc_base,
                alloc,
                trampoline_offset,
                trampoline_size,
            );
        }
    }

    #[cfg(target_arch = "aarch64")]
    #[allow(clippy::too_many_arguments)]
    unsafe fn apply_aarch64_relocation(
        &self,
        name: &str,
        rtype: u32,
        patch_addr: *mut u8,
        s: u64,
        a: u64,
        p: u64,
        alloc_base: *mut u8,
        alloc: &MemoryAllocation,
        trampoline_offset: &mut usize,
        trampoline_size: usize,
        got_offset: &mut usize,
        got_entry_size: usize,
        sym_idx: u32,
    ) {
        match rtype {
            elf::R_AARCH64_ABS64 => {
                (patch_addr as *mut u64).write_unaligned(s.wrapping_add(a));
            }
            elf::R_AARCH64_ABS32 => {
                (patch_addr as *mut u32).write_unaligned(s.wrapping_add(a) as u32);
            }
            elf::R_AARCH64_PREL32 => {
                let val = (s.wrapping_add(a)).wrapping_sub(p) as i32;
                (patch_addr as *mut i32).write_unaligned(val);
            }
            elf::R_AARCH64_MOVW_UABS_G0_NC
            | elf::R_AARCH64_MOVW_UABS_G1_NC
            | elf::R_AARCH64_MOVW_UABS_G2_NC
            | elf::R_AARCH64_MOVW_UABS_G3 => {
                let shift = ((rtype - elf::R_AARCH64_MOVW_UABS_G0_NC) * 16) as u64;
                let insn = (patch_addr as *mut u32).read_unaligned();
                let val16 = ((s.wrapping_add(a)) >> shift) as u16;
                let new_insn = (insn & !(0xFFFF << 5)) | ((val16 as u32) << 5);
                (patch_addr as *mut u32).write_unaligned(new_insn);
            }
            elf::R_AARCH64_ADR_PREL_PG_HI21 => {
                let page = ((s.wrapping_add(a)) & !0xFFF).wrapping_sub(p & !0xFFF) as i64;
                let page_count = page >> 12;
                let insn = (patch_addr as *mut u32).read_unaligned();

                if page_count < -(1i64 << 20) || page_count >= (1i64 << 20) {
                    // ADRP overflow — use GOT entry.
                    if *got_offset + got_entry_size > alloc.size {
                        eprintln!("driverhub: {}: GOT space exhausted for ADRP", name);
                        return;
                    }
                    let got_entry = alloc_base.add(*got_offset);
                    let page_addr = (s.wrapping_add(a)) & !0xFFF;
                    (got_entry as *mut u64).write_unaligned(page_addr);

                    let ldr_offset = (got_entry as i64) - (p as i64);
                    let imm19 = ldr_offset >> 2;
                    if imm19 < -(1i64 << 18) || imm19 >= (1i64 << 18) {
                        eprintln!("driverhub: {}: GOT entry too far for LDR literal", name);
                        return;
                    }

                    let rd = insn & 0x1F;
                    let new_insn =
                        0x58000000u32 | (((imm19 as u32) & 0x7FFFF) << 5) | rd;
                    (patch_addr as *mut u32).write_unaligned(new_insn);

                    eprintln!(
                        "driverhub: {}: ADRP overflow for sym {} using GOT at +{}",
                        name, sym_idx, *got_offset
                    );
                    *got_offset += got_entry_size;
                } else {
                    let immlo = ((page >> 12) & 0x3) as u32;
                    let immhi = ((page >> 14) & 0x7FFFF) as u32;
                    let new_insn = (insn & 0x9F00001F) | (immlo << 29) | (immhi << 5);
                    (patch_addr as *mut u32).write_unaligned(new_insn);
                }
            }
            elf::R_AARCH64_ADD_ABS_LO12_NC | elf::R_AARCH64_LDST8_ABS_LO12_NC => {
                let insn = (patch_addr as *mut u32).read_unaligned();
                let imm12 = (s.wrapping_add(a) & 0xFFF) as u32;
                let new_insn = (insn & !(0xFFF << 10)) | (imm12 << 10);
                (patch_addr as *mut u32).write_unaligned(new_insn);
            }
            elf::R_AARCH64_JUMP26 | elf::R_AARCH64_CALL26 => {
                let offset_val = (s.wrapping_add(a)).wrapping_sub(p) as i64;
                let max_range: i64 = (1 << 27) - 4;
                if offset_val > max_range || offset_val < -max_range {
                    if *trampoline_offset + trampoline_size > alloc.size {
                        eprintln!("driverhub: {}: trampoline space exhausted", name);
                        return;
                    }
                    let tramp = alloc_base.add(*trampoline_offset);
                    (tramp as *mut u32).write_unaligned(0x58000050); // LDR X16, [PC+8]
                    (tramp.add(4) as *mut u32).write_unaligned(0xD61F0200); // BR X16
                    (tramp.add(8) as *mut u64).write_unaligned(s);

                    let t = tramp as u64;
                    let tramp_off = (t as i64) - (p as i64);
                    let insn = (patch_addr as *mut u32).read_unaligned();
                    let new_insn = (insn & 0xFC000000)
                        | (((tramp_off >> 2) as u32) & 0x03FFFFFF);
                    (patch_addr as *mut u32).write_unaligned(new_insn);
                    *trampoline_offset += trampoline_size;
                } else {
                    let insn = (patch_addr as *mut u32).read_unaligned();
                    let new_insn = (insn & 0xFC000000)
                        | (((offset_val >> 2) as u32) & 0x03FFFFFF);
                    (patch_addr as *mut u32).write_unaligned(new_insn);
                }
            }
            elf::R_AARCH64_LDST16_ABS_LO12_NC => {
                let insn = (patch_addr as *mut u32).read_unaligned();
                let imm12 = ((s.wrapping_add(a) & 0xFFF) >> 1) as u32;
                (patch_addr as *mut u32)
                    .write_unaligned((insn & !(0xFFF << 10)) | (imm12 << 10));
            }
            elf::R_AARCH64_LDST32_ABS_LO12_NC => {
                let insn = (patch_addr as *mut u32).read_unaligned();
                let imm12 = ((s.wrapping_add(a) & 0xFFF) >> 2) as u32;
                (patch_addr as *mut u32)
                    .write_unaligned((insn & !(0xFFF << 10)) | (imm12 << 10));
            }
            elf::R_AARCH64_LDST64_ABS_LO12_NC => {
                let insn = (patch_addr as *mut u32).read_unaligned();
                let imm12 = ((s.wrapping_add(a) & 0xFFF) >> 3) as u32;
                (patch_addr as *mut u32)
                    .write_unaligned((insn & !(0xFFF << 10)) | (imm12 << 10));
            }
            elf::R_AARCH64_LDST128_ABS_LO12_NC => {
                let insn = (patch_addr as *mut u32).read_unaligned();
                let imm12 = ((s.wrapping_add(a) & 0xFFF) >> 4) as u32;
                (patch_addr as *mut u32)
                    .write_unaligned((insn & !(0xFFF << 10)) | (imm12 << 10));
            }
            _ => {
                eprintln!(
                    "driverhub: {}: unhandled AArch64 reloc type {}",
                    name, rtype
                );
            }
        }
    }

    #[cfg(not(target_arch = "aarch64"))]
    #[allow(clippy::too_many_arguments)]
    unsafe fn apply_aarch64_relocation(
        &self,
        _name: &str,
        _rtype: u32,
        _patch_addr: *mut u8,
        _s: u64,
        _a: u64,
        _p: u64,
        _alloc_base: *mut u8,
        _alloc: &MemoryAllocation,
        _trampoline_offset: &mut usize,
        _trampoline_size: usize,
        _got_offset: &mut usize,
        _got_entry_size: usize,
        _sym_idx: u32,
    ) {
    }

    #[cfg(target_arch = "x86_64")]
    #[allow(clippy::too_many_arguments)]
    unsafe fn apply_x86_64_relocation(
        &self,
        name: &str,
        rtype: u32,
        patch_addr: *mut u8,
        s: u64,
        a: u64,
        p: u64,
        alloc_base: *mut u8,
        alloc: &MemoryAllocation,
        trampoline_offset: &mut usize,
        trampoline_size: usize,
    ) {
        match rtype {
            elf::R_X86_64_64 => {
                (patch_addr as *mut u64).write_unaligned(s.wrapping_add(a));
            }
            elf::R_X86_64_PC32 | elf::R_X86_64_PLT32 => {
                let rel_val = (s.wrapping_add(a)).wrapping_sub(p) as i64;
                if rel_val > i32::MAX as i64 || rel_val < i32::MIN as i64 {
                    if *trampoline_offset + trampoline_size > alloc.size {
                        eprintln!("driverhub: {}: trampoline space exhausted", name);
                        return;
                    }
                    let tramp = alloc_base.add(*trampoline_offset);
                    *tramp.add(0) = 0xFF;
                    *tramp.add(1) = 0x25;
                    *tramp.add(2) = 0;
                    *tramp.add(3) = 0;
                    *tramp.add(4) = 0;
                    *tramp.add(5) = 0;
                    (tramp.add(6) as *mut u64).write_unaligned(s);

                    let t = tramp as u64;
                    let tramp_rel = (t.wrapping_add(a)).wrapping_sub(p) as i32;
                    (patch_addr as *mut i32).write_unaligned(tramp_rel);
                    *trampoline_offset += trampoline_size;
                } else {
                    (patch_addr as *mut i32).write_unaligned(rel_val as i32);
                }
            }
            elf::R_X86_64_32 => {
                (patch_addr as *mut u32).write_unaligned(s.wrapping_add(a) as u32);
            }
            elf::R_X86_64_32S => {
                (patch_addr as *mut i32).write_unaligned(s.wrapping_add(a) as i32);
            }
            _ => {
                eprintln!(
                    "driverhub: {}: unhandled x86_64 reloc type {}",
                    name, rtype
                );
            }
        }
    }

    #[cfg(not(target_arch = "x86_64"))]
    #[allow(clippy::too_many_arguments)]
    unsafe fn apply_x86_64_relocation(
        &self,
        _name: &str,
        _rtype: u32,
        _patch_addr: *mut u8,
        _s: u64,
        _a: u64,
        _p: u64,
        _alloc_base: *mut u8,
        _alloc: &MemoryAllocation,
        _trampoline_offset: &mut usize,
        _trampoline_size: usize,
    ) {
    }

    /// Patch out kCFI BRK instructions in executable sections.
    #[cfg(target_arch = "aarch64")]
    fn patch_kcfi(shdrs: &[elf::Elf64Shdr], sections: &[LoadedSection]) {
        const NOP: u32 = 0xd503201f;
        const BRK_KCFI_LO: u32 = 0xd4304000;
        const BRK_KCFI_HI: u32 = 0xd4305fe0;
        let mut patched: usize = 0;

        for (i, shdr) in shdrs.iter().enumerate() {
            if shdr.sh_flags & elf::SHF_EXECINSTR == 0 || sections[i].base.is_null() {
                continue;
            }
            let count = shdr.sh_size as usize / 4;
            let code = sections[i].base as *mut u32;
            for j in 0..count {
                let insn = unsafe { code.add(j).read() };
                if insn >= BRK_KCFI_LO && insn <= BRK_KCFI_HI {
                    unsafe { code.add(j).write(NOP) };
                    patched += 1;
                }
            }
        }
        if patched > 0 {
            eprintln!("driverhub: patched {} kCFI checks", patched);
        }
    }

    #[cfg(not(target_arch = "aarch64"))]
    fn patch_kcfi(_shdrs: &[elf::Elf64Shdr], _sections: &[LoadedSection]) {}

    /// Patch privileged MSR/MRS instructions to NOPs or safe equivalents.
    #[cfg(target_arch = "aarch64")]
    fn patch_privileged(shdrs: &[elf::Elf64Shdr], sections: &[LoadedSection]) {
        const NOP: u32 = 0xd503201f;
        let mut patched: usize = 0;

        for (i, shdr) in shdrs.iter().enumerate() {
            if shdr.sh_flags & elf::SHF_EXECINSTR == 0 || sections[i].base.is_null() {
                continue;
            }
            let count = shdr.sh_size as usize / 4;
            let code = sections[i].base as *mut u32;
            for j in 0..count {
                let insn = unsafe { code.add(j).read() };

                // MSR DAIFSet/DAIFClr
                if (insn & 0xFFFFF0DF) == 0xD50340DF {
                    unsafe { code.add(j).write(NOP) };
                    patched += 1;
                    continue;
                }

                // MSR/MRS to EL1 system registers
                if (insn & 0xFFF00000) != 0xD5100000 && (insn & 0xFFF00000) != 0xD5300000 {
                    continue;
                }

                let is_mrs = (insn & (1 << 21)) != 0;
                let rt = insn & 0x1F;

                if is_mrs {
                    // MRS Xn, sysreg → MOV Xn, XZR
                    unsafe { code.add(j).write(0xAA1F03E0 | rt) };
                } else {
                    // MSR sysreg, Xn → NOP
                    unsafe { code.add(j).write(NOP) };
                }
                patched += 1;
            }
        }
        if patched > 0 {
            eprintln!("driverhub: patched {} privileged instructions", patched);
        }
    }

    #[cfg(not(target_arch = "aarch64"))]
    fn patch_privileged(_shdrs: &[elf::Elf64Shdr], _sections: &[LoadedSection]) {}
}
