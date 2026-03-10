// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Parser for Linux `.ko` module `.modinfo` ELF sections.
//!
//! Extracts key=value metadata (description, author, license, vermagic,
//! depends, aliases) from the null-terminated entries in `.modinfo`.

use crate::elf;

/// Metadata extracted from a .ko module's .modinfo section.
#[derive(Debug, Default, Clone)]
pub struct ModuleInfo {
    pub name: String,
    pub description: String,
    pub author: String,
    pub license: String,
    pub vermagic: String,
    pub depends: Vec<String>,
    pub aliases: Vec<String>,
    pub compatible: Vec<String>,
}

/// Parse null-terminated key=value pairs from raw .modinfo section data.
pub fn parse_modinfo(data: &[u8], info: &mut ModuleInfo) {
    let mut pos = 0;
    while pos < data.len() {
        // Skip null bytes (padding between entries).
        if data[pos] == 0 {
            pos += 1;
            continue;
        }

        // Find the end of this null-terminated entry.
        let end = data[pos..]
            .iter()
            .position(|&b| b == 0)
            .map(|i| pos + i)
            .unwrap_or(data.len());

        let entry = match std::str::from_utf8(&data[pos..end]) {
            Ok(s) => s,
            Err(_) => {
                pos = end + 1;
                continue;
            }
        };

        if let Some(eq) = entry.find('=') {
            let key = &entry[..eq];
            let val = &entry[eq + 1..];

            match key {
                "description" => info.description = val.to_string(),
                "author" => info.author = val.to_string(),
                "license" => info.license = val.to_string(),
                "vermagic" => info.vermagic = val.to_string(),
                "depends" => {
                    for dep in val.split(',') {
                        if !dep.is_empty() {
                            info.depends.push(dep.to_string());
                        }
                    }
                }
                "alias" => info.aliases.push(val.to_string()),
                _ => {} // Unknown keys silently ignored.
            }
        }

        pos = end + 1;
    }
}

/// Extract module metadata from a .ko ELF file without fully loading it.
///
/// Parses just the ELF section headers to locate .modinfo, extracts
/// key=value pairs, and populates `info`. Returns `true` on success.
pub fn extract_module_info(data: &[u8], info: &mut ModuleInfo) -> bool {
    let ehdr = match elf::parse_ehdr(data) {
        Some(h) => h,
        None => return false,
    };

    if ehdr.e_type != elf::ET_REL || ehdr.ei_class != elf::ELFCLASS64 {
        return false;
    }

    if ehdr.e_shoff == 0 || ehdr.e_shnum == 0 {
        return false;
    }

    let shdrs = match elf::parse_shdrs(data, &ehdr) {
        Some(s) => s,
        None => return false,
    };

    // Get section name string table.
    let shstrtab_shdr = match shdrs.get(ehdr.e_shstrndx as usize) {
        Some(s) => s,
        None => return false,
    };
    let shstrtab_start = shstrtab_shdr.sh_offset as usize;
    let shstrtab_end = shstrtab_start + shstrtab_shdr.sh_size as usize;
    if shstrtab_end > data.len() {
        return false;
    }
    let shstrtab = &data[shstrtab_start..shstrtab_end];

    // Find .modinfo section.
    for shdr in &shdrs {
        let name = elf::read_str(shstrtab, shdr.sh_name as usize);
        if name == ".modinfo" && shdr.sh_type != elf::SHT_NOBITS {
            let start = shdr.sh_offset as usize;
            let end = start + shdr.sh_size as usize;
            if end <= data.len() {
                parse_modinfo(&data[start..end], info);
            }
            return true;
        }
    }

    // No .modinfo section — still a valid .ko, just no metadata.
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_section() {
        let mut info = ModuleInfo::default();
        parse_modinfo(&[], &mut info);
        assert!(info.description.is_empty());
        assert!(info.depends.is_empty());
    }

    #[test]
    fn single_field() {
        let data = b"license=GPL\0";
        let mut info = ModuleInfo::default();
        parse_modinfo(data, &mut info);
        assert_eq!(info.license, "GPL");
    }

    #[test]
    fn multiple_fields() {
        let data = b"description=Test driver\0author=Test Author\0license=GPL v2\0vermagic=6.6.0 SMP preempt\0";
        let mut info = ModuleInfo::default();
        parse_modinfo(data, &mut info);
        assert_eq!(info.description, "Test driver");
        assert_eq!(info.author, "Test Author");
        assert_eq!(info.license, "GPL v2");
        assert_eq!(info.vermagic, "6.6.0 SMP preempt");
    }

    #[test]
    fn dependencies() {
        let data = b"depends=i2c_core,regmap\0";
        let mut info = ModuleInfo::default();
        parse_modinfo(data, &mut info);
        assert_eq!(info.depends, vec!["i2c_core", "regmap"]);
    }

    #[test]
    fn empty_depends() {
        let data = b"depends=\0";
        let mut info = ModuleInfo::default();
        parse_modinfo(data, &mut info);
        assert!(info.depends.is_empty());
    }

    #[test]
    fn multiple_aliases() {
        let data = b"alias=of:N*T*Cvendor,deviceA\0alias=of:N*T*Cvendor,deviceB\0";
        let mut info = ModuleInfo::default();
        parse_modinfo(data, &mut info);
        assert_eq!(info.aliases.len(), 2);
        assert_eq!(info.aliases[0], "of:N*T*Cvendor,deviceA");
        assert_eq!(info.aliases[1], "of:N*T*Cvendor,deviceB");
    }

    #[test]
    fn unknown_keys_ignored() {
        let data = b"srcversion=ABC123\0license=GPL\0retpoline=Y\0";
        let mut info = ModuleInfo::default();
        parse_modinfo(data, &mut info);
        assert_eq!(info.license, "GPL");
    }

    #[test]
    fn null_byte_padding() {
        let data = b"license=GPL\0\0\0author=Me\0";
        let mut info = ModuleInfo::default();
        parse_modinfo(data, &mut info);
        assert_eq!(info.license, "GPL");
        assert_eq!(info.author, "Me");
    }
}
