// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Pixel Factory Image Extraction Tool
//!
//! Downloads and extracts components from a Pixel factory image:
//! - DTB from vendor_boot.img (via unpack_bootimg)
//! - .ko modules from vendor_dlkm partition (via super.img → lpunpack)
//! - .ko modules from system_dlkm partition
//! - vmlinux.symvers from GKI CI builds
//!
//! Usage:
//!   extract-factory --zip <factory.zip> --output <dir>
//!   extract-factory --url <factory-url> --output <dir>
//!   extract-factory --image-dir <extracted-dir> --output <dir>

use anyhow::{bail, Context, Result};
use clap::Parser;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

/// Pixel factory image extraction tool for DriverHub.
#[derive(Parser, Debug)]
#[command(name = "extract-factory", about = "Extract DTB and modules from Pixel factory images")]
struct Args {
    /// Path to the factory image ZIP file.
    #[arg(long)]
    zip: Option<PathBuf>,

    /// URL to download the factory image ZIP.
    #[arg(long)]
    url: Option<String>,

    /// Path to an already-extracted factory image directory.
    #[arg(long)]
    image_dir: Option<PathBuf>,

    /// Output directory for extracted files.
    #[arg(long, short)]
    output: PathBuf,

    /// Skip downloading, only extract from local files.
    #[arg(long, default_value_t = false)]
    skip_download: bool,

    /// Only extract modules (skip DTB extraction).
    #[arg(long, default_value_t = false)]
    modules_only: bool,

    /// Only extract DTB (skip module extraction).
    #[arg(long, default_value_t = false)]
    dtb_only: bool,

    /// List undefined symbols from each .ko (requires readelf or goblin).
    #[arg(long, default_value_t = false)]
    list_symbols: bool,
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Create output directory.
    fs::create_dir_all(&args.output)
        .with_context(|| format!("Failed to create output dir: {}", args.output.display()))?;

    let work_dir = args.output.join(".work");
    fs::create_dir_all(&work_dir)?;

    // Step 1: Obtain the factory image.
    let image_dir = if let Some(ref dir) = args.image_dir {
        dir.clone()
    } else if let Some(ref zip_path) = args.zip {
        eprintln!("[1/4] Extracting factory ZIP: {}", zip_path.display());
        extract_factory_zip(zip_path, &work_dir)?
    } else if let Some(ref url) = args.url {
        let zip_path = work_dir.join("factory.zip");
        if !args.skip_download || !zip_path.exists() {
            eprintln!("[1/4] Downloading factory image...");
            download_file(url, &zip_path)?;
        } else {
            eprintln!("[1/4] Using cached factory ZIP");
        }
        extract_factory_zip(&zip_path, &work_dir)?
    } else {
        bail!("Must specify one of --zip, --url, or --image-dir");
    };

    eprintln!("[*] Factory image directory: {}", image_dir.display());

    // Step 2: Extract DTB from vendor_boot.img.
    if !args.modules_only {
        eprintln!("[2/4] Extracting DTB from vendor_boot.img...");
        match extract_dtb(&image_dir, &args.output) {
            Ok(dtb_path) => eprintln!("  -> DTB: {}", dtb_path.display()),
            Err(e) => eprintln!("  -> Warning: DTB extraction failed: {e}"),
        }
    }

    // Step 3: Extract modules from super.img → vendor_dlkm / system_dlkm.
    if !args.dtb_only {
        eprintln!("[3/4] Extracting kernel modules from super.img...");
        let modules = extract_modules(&image_dir, &work_dir, &args.output)?;
        eprintln!("  -> Extracted {} modules", modules.len());

        // Step 4: Analyze module symbols if requested.
        if args.list_symbols && !modules.is_empty() {
            eprintln!("[4/4] Analyzing module symbols...");
            analyze_modules(&modules, &args.output)?;
        } else {
            eprintln!("[4/4] Symbol analysis skipped (use --list-symbols to enable)");
        }
    }

    eprintln!("\nDone! Output directory: {}", args.output.display());
    Ok(())
}

/// Download a file from a URL using curl.
fn download_file(url: &str, dest: &Path) -> Result<()> {
    let status = Command::new("curl")
        .args(["-L", "-o"])
        .arg(dest)
        .arg("--progress-bar")
        .arg(url)
        .status()
        .context("Failed to run curl")?;

    if !status.success() {
        bail!("curl failed with status: {status}");
    }
    Ok(())
}

/// Extract a factory ZIP and find the inner image directory.
fn extract_factory_zip(zip_path: &Path, work_dir: &Path) -> Result<PathBuf> {
    let extract_dir = work_dir.join("factory-extracted");
    fs::create_dir_all(&extract_dir)?;

    let file = fs::File::open(zip_path)
        .with_context(|| format!("Cannot open ZIP: {}", zip_path.display()))?;

    let mut archive = zip::ZipArchive::new(file)?;

    // Factory ZIPs contain a single directory with the images.
    // Extract everything.
    for i in 0..archive.len() {
        let mut entry = archive.by_index(i)?;
        let Some(name) = entry.enclosed_name().map(|p| p.to_path_buf()) else {
            continue;
        };
        let out_path = extract_dir.join(&name);

        if entry.is_dir() {
            fs::create_dir_all(&out_path)?;
        } else {
            if let Some(parent) = out_path.parent() {
                fs::create_dir_all(parent)?;
            }
            let mut outfile = fs::File::create(&out_path)?;
            std::io::copy(&mut entry, &mut outfile)?;
        }
    }

    // Find the inner ZIP (e.g., blazer-bd1a.250702.001/image-blazer-bd1a.250702.001.zip)
    // or the directory containing .img files.
    find_image_dir(&extract_dir)
}

/// Find the directory containing .img files (possibly inside a nested ZIP).
fn find_image_dir(dir: &Path) -> Result<PathBuf> {
    // Check if this directory directly contains .img files.
    if has_img_files(dir) {
        return Ok(dir.to_path_buf());
    }

    // Check subdirectories.
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();

        if path.is_dir() {
            if has_img_files(&path) {
                return Ok(path);
            }
            // Check for nested ZIP (image-*.zip).
            for sub in fs::read_dir(&path)? {
                let sub = sub?;
                let sub_path = sub.path();
                if sub_path.extension().map(|e| e == "zip").unwrap_or(false)
                    && sub_path
                        .file_name()
                        .unwrap()
                        .to_str()
                        .map(|n| n.starts_with("image-"))
                        .unwrap_or(false)
                {
                    eprintln!("  Found inner image ZIP: {}", sub_path.display());
                    let inner_dir = path.join("images");
                    fs::create_dir_all(&inner_dir)?;

                    let file = fs::File::open(&sub_path)?;
                    let mut archive = zip::ZipArchive::new(file)?;
                    for i in 0..archive.len() {
                        let mut entry = archive.by_index(i)?;
                        let Some(name) = entry.enclosed_name().map(|p| p.to_path_buf()) else {
                            continue;
                        };
                        let out_path = inner_dir.join(&name);
                        if entry.is_dir() {
                            fs::create_dir_all(&out_path)?;
                        } else {
                            if let Some(parent) = out_path.parent() {
                                fs::create_dir_all(parent)?;
                            }
                            let mut outfile = fs::File::create(&out_path)?;
                            std::io::copy(&mut entry, &mut outfile)?;
                        }
                    }
                    if has_img_files(&inner_dir) {
                        return Ok(inner_dir);
                    }
                }
            }
        }
    }

    bail!("Could not find directory with .img files in {}", dir.display())
}

fn has_img_files(dir: &Path) -> bool {
    fs::read_dir(dir)
        .map(|entries| {
            entries.flatten().any(|e| {
                e.path()
                    .extension()
                    .map(|ext| ext == "img")
                    .unwrap_or(false)
            })
        })
        .unwrap_or(false)
}

/// Extract DTB from vendor_boot.img using unpack_bootimg.
fn extract_dtb(image_dir: &Path, output: &Path) -> Result<PathBuf> {
    let vendor_boot = image_dir.join("vendor_boot.img");
    if !vendor_boot.exists() {
        bail!("vendor_boot.img not found in {}", image_dir.display());
    }

    let dtb_dir = output.join("dtb");
    fs::create_dir_all(&dtb_dir)?;

    // Try unpack_bootimg (from Android build tools).
    let unpack_result = Command::new("unpack_bootimg")
        .args(["--boot_img"])
        .arg(&vendor_boot)
        .arg("--out")
        .arg(&dtb_dir)
        .output();

    match unpack_result {
        Ok(output_cmd) if output_cmd.status.success() => {
            // unpack_bootimg creates dtb in the output dir.
            let dtb_path = dtb_dir.join("dtb");
            if dtb_path.exists() {
                return Ok(dtb_path);
            }
            // Some versions output to different names.
            for entry in fs::read_dir(&dtb_dir)? {
                let entry = entry?;
                let name = entry.file_name();
                let name_str = name.to_string_lossy();
                if name_str.contains("dtb") || name_str.ends_with(".dtb") {
                    return Ok(entry.path());
                }
            }
            bail!("unpack_bootimg succeeded but no dtb file found");
        }
        _ => {
            // Fallback: manual DTB extraction from vendor_boot.img.
            eprintln!("  unpack_bootimg not found, attempting manual DTB extraction...");
            extract_dtb_manual(&vendor_boot, &dtb_dir)
        }
    }
}

/// Manual DTB extraction by scanning for the FDT magic number.
fn extract_dtb_manual(vendor_boot: &Path, dtb_dir: &Path) -> Result<PathBuf> {
    let data = fs::read(vendor_boot)?;

    // DTB magic: 0xd00dfeed (big-endian).
    let magic: [u8; 4] = [0xd0, 0x0d, 0xfe, 0xed];
    let mut found = 0;

    for i in 0..data.len().saturating_sub(4) {
        if data[i..i + 4] == magic {
            // Read totalsize from DTB header (offset 4, big-endian u32).
            if i + 8 > data.len() {
                continue;
            }
            let size = u32::from_be_bytes([data[i + 4], data[i + 5], data[i + 6], data[i + 7]])
                as usize;

            if size < 64 || size > 16 * 1024 * 1024 || i + size > data.len() {
                continue;
            }

            let dtb_path = dtb_dir.join(format!("dtb_{found}.dtb"));
            fs::write(&dtb_path, &data[i..i + size])?;
            eprintln!(
                "  Found DTB at offset 0x{i:x} ({} bytes) -> {}",
                size,
                dtb_path.display()
            );
            found += 1;
        }
    }

    if found == 0 {
        bail!("No DTB found in vendor_boot.img");
    }

    Ok(dtb_dir.join("dtb_0.dtb"))
}

/// Extract kernel modules from super.img via vendor_dlkm and system_dlkm.
fn extract_modules(image_dir: &Path, work_dir: &Path, output: &Path) -> Result<Vec<PathBuf>> {
    let super_img = image_dir.join("super.img");
    if !super_img.exists() {
        bail!("super.img not found in {}", image_dir.display());
    }

    let modules_dir = output.join("modules");
    fs::create_dir_all(&modules_dir)?;

    // Step 1: Convert sparse image to raw (simg2img).
    let raw_super = work_dir.join("super.raw.img");
    if !raw_super.exists() {
        eprintln!("  Converting sparse super.img to raw...");
        run_or_skip("simg2img", &[super_img.to_str().unwrap(), raw_super.to_str().unwrap()])?;
    }

    // Step 2: Extract partitions with lpunpack.
    let partitions_dir = work_dir.join("partitions");
    fs::create_dir_all(&partitions_dir)?;

    let raw_to_unpack = if raw_super.exists() {
        &raw_super
    } else {
        eprintln!("  simg2img not available, trying super.img directly...");
        &super_img
    };

    if !partitions_dir.join("vendor_dlkm.img").exists() {
        eprintln!("  Running lpunpack...");
        let result = Command::new("lpunpack")
            .arg(raw_to_unpack)
            .arg(&partitions_dir)
            .output();

        match result {
            Ok(out) if out.status.success() => {
                eprintln!("  lpunpack succeeded");
            }
            Ok(out) => {
                let stderr = String::from_utf8_lossy(&out.stderr);
                // lpunpack may partially succeed — check if we got our partitions.
                if !partitions_dir.join("vendor_dlkm.img").exists() {
                    eprintln!("  lpunpack warning: {stderr}");
                    eprintln!("  Attempting fallback extraction...");
                    extract_partition_fallback(raw_to_unpack, &partitions_dir)?;
                }
            }
            Err(_) => {
                eprintln!("  lpunpack not found, attempting fallback extraction...");
                extract_partition_fallback(raw_to_unpack, &partitions_dir)?;
            }
        }
    }

    let mut all_modules = Vec::new();

    // Step 3: Mount/extract vendor_dlkm and system_dlkm partitions.
    for partition in ["vendor_dlkm", "system_dlkm"] {
        let part_img = partitions_dir.join(format!("{partition}.img"));
        if !part_img.exists() {
            eprintln!("  {partition}.img not found, skipping");
            continue;
        }

        let part_dir = work_dir.join(partition);
        fs::create_dir_all(&part_dir)?;

        eprintln!("  Extracting modules from {partition}...");
        let modules = extract_modules_from_partition(&part_img, &part_dir, &modules_dir)?;
        eprintln!("    Found {} modules in {partition}", modules.len());
        all_modules.extend(modules);
    }

    Ok(all_modules)
}

/// Extract .ko files from a filesystem image using debugfs or mount.
fn extract_modules_from_partition(
    img: &Path,
    work_dir: &Path,
    modules_dir: &Path,
) -> Result<Vec<PathBuf>> {
    // Try debugfs (ext4) first — works without root.
    let debugfs_result = Command::new("debugfs")
        .args(["-R", "ls -lR /"])
        .arg(img)
        .output();

    if let Ok(output) = debugfs_result {
        if output.status.success() {
            let listing = String::from_utf8_lossy(&output.stdout);
            return extract_ko_via_debugfs(img, &listing, modules_dir);
        }
    }

    // Fallback: Try 7z extraction.
    let result = Command::new("7z")
        .args(["x", "-y"])
        .arg(format!("-o{}", work_dir.display()))
        .arg(img)
        .output();

    if let Ok(output) = result {
        if output.status.success() {
            return find_ko_files(work_dir, modules_dir);
        }
    }

    // Fallback: binary scan for ELF headers (last resort).
    eprintln!("    No extraction tool available, scanning for ELF .ko files...");
    extract_ko_binary_scan(img, modules_dir)
}

/// Use debugfs to extract .ko files from an ext4 image.
fn extract_ko_via_debugfs(img: &Path, listing: &str, modules_dir: &Path) -> Result<Vec<PathBuf>> {
    let mut modules = Vec::new();

    for line in listing.lines() {
        let line = line.trim();
        if !line.contains(".ko") {
            continue;
        }
        // debugfs ls output format varies, extract paths ending in .ko.
        let parts: Vec<&str> = line.split_whitespace().collect();
        if let Some(path) = parts.last() {
            if path.ends_with(".ko") {
                let filename = Path::new(path)
                    .file_name()
                    .unwrap_or_default()
                    .to_string_lossy();
                let dest = modules_dir.join(filename.as_ref());

                let dump = Command::new("debugfs")
                    .args(["-R", &format!("dump {path} {}", dest.display())])
                    .arg(img)
                    .output();

                if let Ok(out) = dump {
                    if out.status.success() && dest.exists() {
                        modules.push(dest);
                    }
                }
            }
        }
    }

    Ok(modules)
}

/// Find .ko files recursively in a directory and copy to modules_dir.
fn find_ko_files(dir: &Path, modules_dir: &Path) -> Result<Vec<PathBuf>> {
    let mut modules = Vec::new();
    find_ko_recursive(dir, modules_dir, &mut modules)?;
    Ok(modules)
}

fn find_ko_recursive(dir: &Path, modules_dir: &Path, modules: &mut Vec<PathBuf>) -> Result<()> {
    if !dir.is_dir() {
        return Ok(());
    }
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        if path.is_dir() {
            find_ko_recursive(&path, modules_dir, modules)?;
        } else if path.extension().map(|e| e == "ko").unwrap_or(false) {
            let dest = modules_dir.join(path.file_name().unwrap());
            fs::copy(&path, &dest)?;
            modules.push(dest);
        }
    }
    Ok(())
}

/// Binary scan for ELF relocatable objects (.ko files) in a raw image.
fn extract_ko_binary_scan(img: &Path, modules_dir: &Path) -> Result<Vec<PathBuf>> {
    let data = fs::read(img).context("Failed to read partition image")?;
    let mut modules = Vec::new();
    let elf_magic: [u8; 4] = [0x7f, b'E', b'L', b'F'];

    let mut offset = 0;
    while offset + 64 < data.len() {
        if data[offset..offset + 4] != elf_magic {
            offset += 4096; // Scan in page increments.
            continue;
        }

        // Check if this is ET_REL (relocatable, type 1) for AArch64 (machine 0xB7).
        let ei_class = data[offset + 4]; // 1=32bit, 2=64bit
        if ei_class != 2 {
            offset += 4096;
            continue;
        }

        let e_type = u16::from_le_bytes([data[offset + 16], data[offset + 17]]);
        let e_machine = u16::from_le_bytes([data[offset + 18], data[offset + 19]]);

        if e_type != 1 || e_machine != 0xB7 {
            offset += 4096;
            continue;
        }

        // Read section header info to determine total file size.
        let e_shoff = u64::from_le_bytes(
            data[offset + 40..offset + 48].try_into().unwrap(),
        ) as usize;
        let e_shentsize = u16::from_le_bytes([data[offset + 58], data[offset + 59]]) as usize;
        let e_shnum = u16::from_le_bytes([data[offset + 60], data[offset + 61]]) as usize;

        if e_shoff == 0 || e_shentsize == 0 || e_shnum == 0 {
            offset += 4096;
            continue;
        }

        let total_size = e_shoff + (e_shentsize * e_shnum);
        if offset + total_size > data.len() || total_size > 64 * 1024 * 1024 {
            offset += 4096;
            continue;
        }

        // Try to extract the module name from .modinfo section.
        let name = extract_module_name(&data[offset..offset + total_size])
            .unwrap_or_else(|| format!("module_{:08x}", offset));

        let dest = modules_dir.join(format!("{name}.ko"));
        fs::write(&dest, &data[offset..offset + total_size])?;
        modules.push(dest);

        offset += total_size;
    }

    Ok(modules)
}

/// Try to extract the module name from a .ko's .modinfo section.
fn extract_module_name(elf_data: &[u8]) -> Option<String> {
    match goblin::elf::Elf::parse(elf_data) {
        Ok(elf) => {
            // Find .modinfo section.
            for section in &elf.section_headers {
                let name = elf.shdr_strtab.get_at(section.sh_name)?;
                if name == ".modinfo" {
                    let start = section.sh_offset as usize;
                    let end = start + section.sh_size as usize;
                    if end <= elf_data.len() {
                        let modinfo = &elf_data[start..end];
                        // Parse key=value\0 entries.
                        for entry in modinfo.split(|&b| b == 0) {
                            if let Ok(s) = std::str::from_utf8(entry) {
                                if let Some(name) = s.strip_prefix("name=") {
                                    return Some(name.to_string());
                                }
                            }
                        }
                    }
                }
            }
            None
        }
        Err(_) => None,
    }
}

/// Fallback partition extraction using binary scanning for ext4 superblock.
fn extract_partition_fallback(super_img: &Path, partitions_dir: &Path) -> Result<()> {
    eprintln!("    Fallback: scanning super.img for ext4 partitions...");
    let data = fs::read(super_img).context("Failed to read super.img")?;

    // ext4 superblock magic at offset 0x438 from partition start: 0x53EF
    let mut found = 0;
    let mut offset = 0;
    while offset + 0x440 < data.len() {
        let magic = u16::from_le_bytes([data[offset + 0x438], data[offset + 0x439]]);
        if magic == 0xEF53 {
            // Found ext4 superblock. Read block count and block size to determine partition size.
            let block_count = u32::from_le_bytes(
                data[offset + 0x404..offset + 0x408].try_into().unwrap(),
            ) as u64;
            let log_block_size = u32::from_le_bytes(
                data[offset + 0x418..offset + 0x41c].try_into().unwrap(),
            );
            let block_size = 1024u64 << log_block_size;
            let part_size = (block_count * block_size) as usize;

            if part_size < data.len() - offset && part_size > 1024 * 1024 {
                let name = match found {
                    0 => "vendor_dlkm",
                    1 => "system_dlkm",
                    _ => &format!("partition_{found}"),
                };
                let dest = partitions_dir.join(format!("{name}.img"));
                eprintln!(
                    "    Found ext4 partition at offset 0x{offset:x} ({} MB)",
                    part_size / (1024 * 1024)
                );
                fs::write(&dest, &data[offset..offset + part_size])?;
                found += 1;
            }
            offset += part_size.max(4096);
        } else {
            offset += 4096;
        }
    }

    if found == 0 {
        bail!("No ext4 partitions found in super.img");
    }
    Ok(())
}

/// Analyze modules: list undefined symbols using goblin.
fn analyze_modules(modules: &[PathBuf], output: &Path) -> Result<()> {
    let report_path = output.join("module_symbols.txt");
    let mut report = String::new();
    let mut all_undefined: HashMap<String, Vec<String>> = HashMap::new();

    for module_path in modules {
        let module_name = module_path
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
            .to_string();

        let data = fs::read(module_path)?;
        match goblin::elf::Elf::parse(&data) {
            Ok(elf) => {
                let mut undefined = Vec::new();
                let mut exported = Vec::new();

                for sym in &elf.syms {
                    if let Some(name) = elf.strtab.get_at(sym.st_name) {
                        if name.is_empty() {
                            continue;
                        }
                        let is_global = sym.st_bind() == goblin::elf::sym::STB_GLOBAL
                            || sym.st_bind() == goblin::elf::sym::STB_WEAK;
                        if sym.st_shndx == goblin::elf::section_header::SHN_UNDEF as usize
                            && is_global
                        {
                            undefined.push(name.to_string());
                        } else if is_global
                            && sym.st_shndx != goblin::elf::section_header::SHN_UNDEF as usize
                        {
                            exported.push(name.to_string());
                        }
                    }
                }

                report.push_str(&format!(
                    "\n=== {} ===\n  Exported: {}, Undefined: {}\n",
                    module_name,
                    exported.len(),
                    undefined.len()
                ));

                if !undefined.is_empty() {
                    report.push_str("  Undefined symbols:\n");
                    for sym in &undefined {
                        report.push_str(&format!("    {sym}\n"));
                        all_undefined
                            .entry(sym.clone())
                            .or_default()
                            .push(module_name.clone());
                    }
                }
            }
            Err(e) => {
                report.push_str(&format!("\n=== {} ===\n  Parse error: {e}\n", module_name));
            }
        }
    }

    // Summary: most demanded symbols.
    report.push_str("\n\n=== MOST DEMANDED UNDEFINED SYMBOLS ===\n");
    let mut sorted: Vec<_> = all_undefined.iter().collect();
    sorted.sort_by(|a, b| b.1.len().cmp(&a.1.len()));

    for (sym, modules) in sorted.iter().take(50) {
        report.push_str(&format!("  {} (needed by {} modules)\n", sym, modules.len()));
    }

    fs::write(&report_path, &report)?;
    eprintln!("  Report: {}", report_path.display());
    eprintln!(
        "  Total unique undefined symbols: {}",
        all_undefined.len()
    );

    Ok(())
}

/// Run a command, or print a warning if the tool is not available.
fn run_or_skip(cmd: &str, args: &[&str]) -> Result<()> {
    let result = Command::new(cmd).args(args).output();
    match result {
        Ok(output) if output.status.success() => Ok(()),
        Ok(output) => {
            let stderr = String::from_utf8_lossy(&output.stderr);
            bail!("{cmd} failed: {stderr}");
        }
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
            eprintln!("  Warning: {cmd} not found, skipping");
            Ok(())
        }
        Err(e) => bail!("Failed to run {cmd}: {e}"),
    }
}
