// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Device tree (FDT) blob parser for module discovery.
//!
//! Parses a Flattened Device Tree blob to extract module entries with
//! compatible strings, MMIO resources, and IRQ resources.

/// FDT magic number (big-endian 0xD00DFEED).
const FDT_MAGIC: u32 = 0xD00D_FEED;
const FDT_BEGIN_NODE: u32 = 0x0000_0001;
const FDT_END_NODE: u32 = 0x0000_0002;
const FDT_PROP: u32 = 0x0000_0003;
const FDT_END: u32 = 0x0000_0009;

/// A hardware resource extracted from a DT node.
#[derive(Debug, Clone)]
pub enum Resource {
    Mmio { base: u64, size: u64 },
    Irq { number: u64 },
}

/// A device tree node describing a module to load.
#[derive(Debug, Clone, Default)]
pub struct DtModuleEntry {
    pub compatible: String,
    pub module_path: String,
    pub properties: Vec<(String, Vec<u8>)>,
    pub resources: Vec<Resource>,
}

/// Read a big-endian u32.
fn read_be32(data: &[u8], off: usize) -> u32 {
    u32::from_be_bytes([data[off], data[off + 1], data[off + 2], data[off + 3]])
}

/// Read a big-endian u64.
fn read_be64(data: &[u8], off: usize) -> u64 {
    u64::from_be_bytes([
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

/// Align offset to 4-byte boundary.
fn align4(offset: usize) -> usize {
    (offset + 3) & !3
}

/// Read a null-terminated C string from `data` starting at `off`.
fn read_cstr(data: &[u8], off: usize) -> &str {
    let end = data[off..]
        .iter()
        .position(|&b| b == 0)
        .map(|i| off + i)
        .unwrap_or(data.len());
    std::str::from_utf8(&data[off..end]).unwrap_or("")
}

/// Parse a static DTB blob and extract module entries.
///
/// Returns entries for depth-2+ nodes that have a `compatible` property
/// and either no `status` property or `status = "okay"`.
pub fn parse_static_dtb(dtb: &[u8]) -> Vec<DtModuleEntry> {
    let mut entries = Vec::new();

    if dtb.len() < 40 {
        eprintln!("driverhub: dt: blob too small ({} bytes)", dtb.len());
        return entries;
    }

    let magic = read_be32(dtb, 0);
    if magic != FDT_MAGIC {
        eprintln!(
            "driverhub: dt: bad magic 0x{:08x} (expected 0x{:08x})",
            magic, FDT_MAGIC
        );
        return entries;
    }

    let off_dt_struct = read_be32(dtb, 8) as usize;
    let off_dt_strings = read_be32(dtb, 12) as usize;
    let size_dt_struct = read_be32(dtb, 36) as usize;

    let struct_base = off_dt_struct;
    let strings_base = off_dt_strings;

    let mut offset: usize = 0;
    let mut current = DtModuleEntry::default();
    let mut current_status = String::new();
    let mut in_node = false;
    let mut depth: i32 = 0;
    let mut target_depth: i32 = -1;

    while offset < size_dt_struct {
        if struct_base + offset + 4 > dtb.len() {
            break;
        }
        let token = read_be32(dtb, struct_base + offset);
        offset += 4;

        match token {
            FDT_BEGIN_NODE => {
                let name_start = struct_base + offset;
                if name_start >= dtb.len() {
                    break;
                }
                let name = read_cstr(dtb, name_start);
                let name_len = name.len();
                offset = align4(offset + name_len + 1);
                depth += 1;

                if depth >= 2 && !in_node {
                    current = DtModuleEntry::default();
                    current_status.clear();
                    in_node = true;
                    target_depth = depth;
                }
            }
            FDT_END_NODE => {
                if in_node && depth == target_depth {
                    if !current.compatible.is_empty()
                        && (current_status.is_empty() || current_status == "okay")
                    {
                        // Derive module path from compatible string.
                        let mut mod_name = current.compatible.clone();
                        if let Some(comma) = mod_name.find(',') {
                            mod_name = mod_name[comma + 1..].to_string();
                        }
                        mod_name = mod_name.replace('-', "_");
                        current.module_path = format!("{}.ko", mod_name);
                        entries.push(current.clone());
                    }
                    in_node = false;
                    target_depth = -1;
                }
                depth -= 1;
            }
            FDT_PROP => {
                if struct_base + offset + 8 > dtb.len() {
                    break;
                }
                let len = read_be32(dtb, struct_base + offset) as usize;
                let nameoff = read_be32(dtb, struct_base + offset + 4) as usize;
                offset += 8;

                let prop_name = if strings_base + nameoff < dtb.len() {
                    read_cstr(dtb, strings_base + nameoff)
                } else {
                    ""
                };

                let prop_start = struct_base + offset;
                let prop_data = if prop_start + len <= dtb.len() {
                    &dtb[prop_start..prop_start + len]
                } else {
                    &[]
                };
                offset = align4(offset + len);

                if !in_node || depth != target_depth {
                    continue;
                }

                match prop_name {
                    "compatible" if !prop_data.is_empty() => {
                        current.compatible = read_cstr(prop_data, 0).to_string();
                    }
                    "status" if !prop_data.is_empty() => {
                        current_status = read_cstr(prop_data, 0).to_string();
                    }
                    "reg" => {
                        // Pairs of (address, size) as 64-bit cells.
                        let mut i = 0;
                        while i + 15 < len {
                            let base = read_be64(prop_data, i);
                            let size = read_be64(prop_data, i + 8);
                            current.resources.push(Resource::Mmio { base, size });
                            i += 16;
                        }
                    }
                    "interrupts" => {
                        // Each cell is a 32-bit IRQ number.
                        let mut i = 0;
                        while i + 3 < len {
                            let num = read_be32(prop_data, i) as u64;
                            current.resources.push(Resource::Irq { number: num });
                            i += 4;
                        }
                    }
                    _ => {
                        current
                            .properties
                            .push((prop_name.to_string(), prop_data.to_vec()));
                    }
                }
            }
            FDT_END => break,
            _ => {} // NOP or unknown tokens.
        }
    }

    eprintln!(
        "driverhub: dt: parsed {} module entries from DTB",
        entries.len()
    );
    entries
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Build a minimal FDT blob for testing.
    fn build_test_dtb(nodes: &[(&str, &str, &str)]) -> Vec<u8> {
        // nodes: (node_name, compatible, status)
        // Build a minimal DTB with a root node and child nodes.

        let mut strings = Vec::new();
        let mut string_offsets = std::collections::HashMap::new();

        // Pre-register property names.
        for name in &["compatible", "status", "reg", "interrupts"] {
            let off = strings.len();
            string_offsets.insert(*name, off as u32);
            strings.extend_from_slice(name.as_bytes());
            strings.push(0);
        }

        let mut structure = Vec::new();

        // Helper to push BE32.
        let push32 = |v: &mut Vec<u8>, val: u32| v.extend_from_slice(&val.to_be_bytes());

        // Root node.
        push32(&mut structure, FDT_BEGIN_NODE);
        structure.push(0); // empty root name
        while structure.len() % 4 != 0 {
            structure.push(0);
        }

        for (name, compat, status) in nodes {
            // Begin child node.
            push32(&mut structure, FDT_BEGIN_NODE);
            structure.extend_from_slice(name.as_bytes());
            structure.push(0);
            while structure.len() % 4 != 0 {
                structure.push(0);
            }

            // compatible property.
            if !compat.is_empty() {
                push32(&mut structure, FDT_PROP);
                let val = format!("{}\0", compat);
                push32(&mut structure, val.len() as u32);
                push32(&mut structure, string_offsets["compatible"]);
                structure.extend_from_slice(val.as_bytes());
                while structure.len() % 4 != 0 {
                    structure.push(0);
                }
            }

            // status property.
            if !status.is_empty() {
                push32(&mut structure, FDT_PROP);
                let val = format!("{}\0", status);
                push32(&mut structure, val.len() as u32);
                push32(&mut structure, string_offsets["status"]);
                structure.extend_from_slice(val.as_bytes());
                while structure.len() % 4 != 0 {
                    structure.push(0);
                }
            }

            push32(&mut structure, FDT_END_NODE);
        }

        push32(&mut structure, FDT_END_NODE); // end root
        push32(&mut structure, FDT_END);

        // Build header (40 bytes).
        let off_dt_struct = 40u32;
        let off_dt_strings = 40 + structure.len() as u32;
        let totalsize = off_dt_strings + strings.len() as u32;

        let mut dtb = Vec::new();
        push32(&mut dtb, FDT_MAGIC);
        push32(&mut dtb, totalsize);
        push32(&mut dtb, off_dt_struct);
        push32(&mut dtb, off_dt_strings);
        push32(&mut dtb, 0); // off_mem_rsvmap
        push32(&mut dtb, 17); // version
        push32(&mut dtb, 16); // last_comp_version
        push32(&mut dtb, 0); // boot_cpuid_phys
        push32(&mut dtb, strings.len() as u32);
        push32(&mut dtb, structure.len() as u32);

        dtb.extend_from_slice(&structure);
        dtb.extend_from_slice(&strings);

        dtb
    }

    #[test]
    fn parse_empty_dtb() {
        let dtb = build_test_dtb(&[]);
        let entries = parse_static_dtb(&dtb);
        assert!(entries.is_empty());
    }

    #[test]
    fn parse_single_node() {
        let dtb = build_test_dtb(&[("rtc@0", "test,rtc-test", "")]);
        let entries = parse_static_dtb(&dtb);
        assert_eq!(entries.len(), 1);
        assert_eq!(entries[0].compatible, "test,rtc-test");
        assert_eq!(entries[0].module_path, "rtc_test.ko");
    }

    #[test]
    fn parse_status_okay() {
        let dtb = build_test_dtb(&[("dev@0", "vendor,dev", "okay")]);
        let entries = parse_static_dtb(&dtb);
        assert_eq!(entries.len(), 1);
    }

    #[test]
    fn skip_status_disabled() {
        let dtb = build_test_dtb(&[("dev@0", "vendor,dev", "disabled")]);
        let entries = parse_static_dtb(&dtb);
        assert!(entries.is_empty());
    }

    #[test]
    fn multiple_nodes() {
        let dtb = build_test_dtb(&[
            ("rtc@0", "test,rtc-test", ""),
            ("gpio@1", "test,gpio-ctrl", "okay"),
        ]);
        let entries = parse_static_dtb(&dtb);
        assert_eq!(entries.len(), 2);
        assert_eq!(entries[0].module_path, "rtc_test.ko");
        assert_eq!(entries[1].module_path, "gpio_ctrl.ko");
    }

    #[test]
    fn blob_too_small() {
        let entries = parse_static_dtb(&[0; 10]);
        assert!(entries.is_empty());
    }

    #[test]
    fn bad_magic() {
        let mut dtb = vec![0u8; 40];
        dtb[0..4].copy_from_slice(&0xDEADBEEFu32.to_be_bytes());
        let entries = parse_static_dtb(&dtb);
        assert!(entries.is_empty());
    }
}
