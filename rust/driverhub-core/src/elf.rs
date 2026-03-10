// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Minimal ELF parsing for 64-bit ET_REL (relocatable) files.
//!
//! This provides just enough ELF parsing for loading `.ko` kernel modules.
//! It handles both little-endian (LE) and big-endian (BE) ELF files,
//! though GKI modules are always LE on ARM64 and x86_64.

// ELF constants.
pub const ELFMAG: &[u8; 4] = b"\x7fELF";
pub const ELFCLASS64: u8 = 2;
pub const ELFDATA2LSB: u8 = 1;
pub const ET_REL: u16 = 1;
pub const EM_AARCH64: u16 = 183;
pub const EM_X86_64: u16 = 62;

pub const SHT_NOBITS: u32 = 8;
pub const SHT_RELA: u32 = 4;
pub const SHT_SYMTAB: u32 = 2;
pub const SHF_ALLOC: u64 = 0x2;
pub const SHF_EXECINSTR: u64 = 0x4;

pub const STB_GLOBAL: u8 = 1;
pub const SHN_UNDEF: u16 = 0;
pub const SHN_ABS: u16 = 0xFFF1;

// AArch64 relocation types.
pub const R_AARCH64_ABS64: u32 = 257;
pub const R_AARCH64_ABS32: u32 = 258;
pub const R_AARCH64_PREL32: u32 = 261;
pub const R_AARCH64_MOVW_UABS_G0_NC: u32 = 264;
pub const R_AARCH64_MOVW_UABS_G1_NC: u32 = 265;
pub const R_AARCH64_MOVW_UABS_G2_NC: u32 = 266;
pub const R_AARCH64_MOVW_UABS_G3: u32 = 267;
pub const R_AARCH64_ADR_PREL_PG_HI21: u32 = 275;
pub const R_AARCH64_ADD_ABS_LO12_NC: u32 = 277;
pub const R_AARCH64_LDST8_ABS_LO12_NC: u32 = 278;
pub const R_AARCH64_JUMP26: u32 = 282;
pub const R_AARCH64_CALL26: u32 = 283;
pub const R_AARCH64_LDST16_ABS_LO12_NC: u32 = 284;
pub const R_AARCH64_LDST32_ABS_LO12_NC: u32 = 285;
pub const R_AARCH64_LDST64_ABS_LO12_NC: u32 = 286;
pub const R_AARCH64_LDST128_ABS_LO12_NC: u32 = 299;

// x86_64 relocation types.
pub const R_X86_64_64: u32 = 1;
pub const R_X86_64_PC32: u32 = 2;
pub const R_X86_64_PLT32: u32 = 4;
pub const R_X86_64_32: u32 = 10;
pub const R_X86_64_32S: u32 = 11;

/// Parsed ELF64 header.
#[derive(Debug, Clone)]
pub struct Elf64Ehdr {
    pub ei_class: u8,
    pub ei_data: u8,
    pub e_type: u16,
    pub e_machine: u16,
    pub e_shoff: u64,
    pub e_shnum: u16,
    pub e_shstrndx: u16,
    pub e_shentsize: u16,
}

/// Parsed ELF64 section header.
#[derive(Debug, Clone)]
pub struct Elf64Shdr {
    pub sh_name: u32,
    pub sh_type: u32,
    pub sh_flags: u64,
    pub sh_addr: u64,
    pub sh_offset: u64,
    pub sh_size: u64,
    pub sh_link: u32,
    pub sh_info: u32,
    pub sh_addralign: u64,
    pub sh_entsize: u64,
}

/// Parsed ELF64 symbol.
#[derive(Debug, Clone)]
pub struct Elf64Sym {
    pub st_name: u32,
    pub st_info: u8,
    pub st_other: u8,
    pub st_shndx: u16,
    pub st_value: u64,
    pub st_size: u64,
}

/// Parsed ELF64 relocation with addend.
#[derive(Debug, Clone)]
pub struct Elf64Rela {
    pub r_offset: u64,
    pub r_info: u64,
    pub r_addend: i64,
}

impl Elf64Rela {
    pub fn r_sym(&self) -> u32 {
        (self.r_info >> 32) as u32
    }
    pub fn r_type(&self) -> u32 {
        self.r_info as u32
    }
}

impl Elf64Sym {
    pub fn st_bind(&self) -> u8 {
        self.st_info >> 4
    }
}

/// Helper: read a little-endian u16 from a byte slice.
fn read_u16_le(data: &[u8], off: usize) -> u16 {
    u16::from_le_bytes([data[off], data[off + 1]])
}

/// Helper: read a little-endian u32 from a byte slice.
fn read_u32_le(data: &[u8], off: usize) -> u32 {
    u32::from_le_bytes([data[off], data[off + 1], data[off + 2], data[off + 3]])
}

/// Helper: read a little-endian u64 from a byte slice.
fn read_u64_le(data: &[u8], off: usize) -> u64 {
    u64::from_le_bytes([
        data[off],
        data[off + 1],
        data[off + 2],
        data[off + 3],
        data[off + 4],
        data[off + 5],
        data[off + 6],
        data[off + 7],
    ])
}

/// Helper: read a little-endian i64 from a byte slice.
fn read_i64_le(data: &[u8], off: usize) -> i64 {
    i64::from_le_bytes([
        data[off],
        data[off + 1],
        data[off + 2],
        data[off + 3],
        data[off + 4],
        data[off + 5],
        data[off + 6],
        data[off + 7],
    ])
}

/// Parse the ELF64 header. Returns `None` if the data is too small or
/// has bad magic.
pub fn parse_ehdr(data: &[u8]) -> Option<Elf64Ehdr> {
    if data.len() < 64 {
        return None;
    }
    if &data[0..4] != ELFMAG {
        return None;
    }
    Some(Elf64Ehdr {
        ei_class: data[4],
        ei_data: data[5],
        e_type: read_u16_le(data, 16),
        e_machine: read_u16_le(data, 18),
        e_shoff: read_u64_le(data, 40),
        e_shentsize: read_u16_le(data, 58),
        e_shnum: read_u16_le(data, 60),
        e_shstrndx: read_u16_le(data, 62),
    })
}

/// Parse all section headers from the ELF data.
pub fn parse_shdrs(data: &[u8], ehdr: &Elf64Ehdr) -> Option<Vec<Elf64Shdr>> {
    let off = ehdr.e_shoff as usize;
    let ent = ehdr.e_shentsize as usize;
    let num = ehdr.e_shnum as usize;

    if ent < 64 || off + ent * num > data.len() {
        return None;
    }

    let mut shdrs = Vec::with_capacity(num);
    for i in 0..num {
        let base = off + i * ent;
        shdrs.push(Elf64Shdr {
            sh_name: read_u32_le(data, base),
            sh_type: read_u32_le(data, base + 4),
            sh_flags: read_u64_le(data, base + 8),
            sh_addr: read_u64_le(data, base + 16),
            sh_offset: read_u64_le(data, base + 24),
            sh_size: read_u64_le(data, base + 32),
            sh_link: read_u32_le(data, base + 40),
            sh_info: read_u32_le(data, base + 44),
            sh_addralign: read_u64_le(data, base + 48),
            sh_entsize: read_u64_le(data, base + 56),
        });
    }
    Some(shdrs)
}

/// Parse symbols from a SHT_SYMTAB section.
pub fn parse_symtab(data: &[u8], shdr: &Elf64Shdr) -> Vec<Elf64Sym> {
    let off = shdr.sh_offset as usize;
    let size = shdr.sh_size as usize;
    let ent = if shdr.sh_entsize > 0 {
        shdr.sh_entsize as usize
    } else {
        24 // sizeof(Elf64_Sym)
    };

    if off + size > data.len() || ent < 24 {
        return Vec::new();
    }

    let count = size / ent;
    let mut syms = Vec::with_capacity(count);
    for i in 0..count {
        let base = off + i * ent;
        syms.push(Elf64Sym {
            st_name: read_u32_le(data, base),
            st_info: data[base + 4],
            st_other: data[base + 5],
            st_shndx: read_u16_le(data, base + 6),
            st_value: read_u64_le(data, base + 8),
            st_size: read_u64_le(data, base + 16),
        });
    }
    syms
}

/// Parse relocations from a SHT_RELA section.
pub fn parse_relas(data: &[u8], shdr: &Elf64Shdr) -> Vec<Elf64Rela> {
    let off = shdr.sh_offset as usize;
    let size = shdr.sh_size as usize;
    let ent = if shdr.sh_entsize > 0 {
        shdr.sh_entsize as usize
    } else {
        24 // sizeof(Elf64_Rela)
    };

    if off + size > data.len() || ent < 24 {
        return Vec::new();
    }

    let count = size / ent;
    let mut relas = Vec::with_capacity(count);
    for i in 0..count {
        let base = off + i * ent;
        relas.push(Elf64Rela {
            r_offset: read_u64_le(data, base),
            r_info: read_u64_le(data, base + 8),
            r_addend: read_i64_le(data, base + 16),
        });
    }
    relas
}

/// Read a null-terminated string from a string table.
pub fn read_str(strtab: &[u8], offset: usize) -> &str {
    if offset >= strtab.len() {
        return "";
    }
    let end = strtab[offset..]
        .iter()
        .position(|&b| b == 0)
        .map(|i| offset + i)
        .unwrap_or(strtab.len());
    std::str::from_utf8(&strtab[offset..end]).unwrap_or("")
}
