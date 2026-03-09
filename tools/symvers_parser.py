#!/usr/bin/env python3
# Copyright 2024 The DriverHub Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parse GKI vmlinux.symvers and compare against DriverHub's symbol registry.

Usage:
  # Compare against our registry (list of known symbols):
  ./tools/symvers_parser.py --symvers vmlinux.symvers --registry src/symbols/symbol_registry.cc

  # Check which symbols a specific .ko module needs:
  ./tools/symvers_parser.py --symvers vmlinux.symvers --module path/to/module.ko

  # Dump all symbols from symvers:
  ./tools/symvers_parser.py --symvers vmlinux.symvers --dump

  # Generate stub code for missing symbols:
  ./tools/symvers_parser.py --symvers vmlinux.symvers --registry src/symbols/symbol_registry.cc --generate-stubs

vmlinux.symvers format (tab-separated):
  <CRC>  <symbol>  <namespace>  <module>  <export_type>

Example:
  0x12345678  kmalloc  (null)  vmlinux  EXPORT_SYMBOL_GPL
"""

import argparse
import os
import re
import struct
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class SymversEntry:
    crc: int
    name: str
    namespace: str
    module: str
    export_type: str  # EXPORT_SYMBOL or EXPORT_SYMBOL_GPL


@dataclass
class ModuleSymbol:
    name: str
    is_undefined: bool  # True = module needs this symbol
    is_global: bool
    section: str = ""


def parse_symvers(path: str) -> dict[str, SymversEntry]:
    """Parse a vmlinux.symvers or Module.symvers file."""
    symbols = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 4:
                # Some older formats use spaces
                parts = line.split()
            if len(parts) < 4:
                continue

            crc_str = parts[0].strip()
            name = parts[1].strip()
            # Handle varying formats
            if len(parts) >= 5:
                namespace = parts[2].strip()
                module = parts[3].strip()
                export_type = parts[4].strip()
            elif len(parts) == 4:
                namespace = ""
                module = parts[2].strip()
                export_type = parts[3].strip()
            else:
                continue

            try:
                crc = int(crc_str, 0)
            except ValueError:
                crc = 0

            symbols[name] = SymversEntry(
                crc=crc,
                name=name,
                namespace=namespace,
                module=module,
                export_type=export_type,
            )
    return symbols


def parse_registry(path: str) -> set[str]:
    """Extract registered symbol names from symbol_registry.cc."""
    symbols = set()
    with open(path) as f:
        content = f.read()

    # Match REGISTER_SYMBOL(name) calls
    for m in re.finditer(r'REGISTER_SYMBOL\(\s*(\w+)\s*\)', content):
        symbols.add(m.group(1))

    # Match Register("name", ...) calls
    for m in re.finditer(r'Register\(\s*"(\w+)"', content):
        symbols.add(m.group(1))

    return symbols


def parse_module_symbols(ko_path: str) -> list[ModuleSymbol]:
    """Extract symbol table from a .ko ELF file using readelf or objdump."""
    symbols = []

    # Try readelf first
    try:
        result = subprocess.run(
            ["readelf", "-sW", ko_path],
            capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0:
            return _parse_readelf_output(result.stdout)
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    # Try llvm-readelf
    try:
        result = subprocess.run(
            ["llvm-readelf", "-sW", ko_path],
            capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0:
            return _parse_readelf_output(result.stdout)
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    # Try aarch64-linux-gnu-readelf
    try:
        result = subprocess.run(
            ["aarch64-linux-gnu-readelf", "-sW", ko_path],
            capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0:
            return _parse_readelf_output(result.stdout)
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    # Fallback: parse ELF directly (basic)
    return _parse_elf_symbols(ko_path)


def _parse_readelf_output(output: str) -> list[ModuleSymbol]:
    """Parse readelf -sW output."""
    symbols = []
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 8:
            continue
        # Format: Num Value Size Type Bind Vis Ndx Name
        try:
            ndx = parts[6]
            name = parts[7]
            bind = parts[4]
            section = ndx
        except (IndexError, ValueError):
            continue

        if not name or name.startswith("$"):
            continue

        is_undefined = (ndx == "UND")
        is_global = (bind == "GLOBAL" or bind == "WEAK")
        symbols.append(ModuleSymbol(
            name=name,
            is_undefined=is_undefined,
            is_global=is_global,
            section=section,
        ))
    return symbols


def _parse_elf_symbols(ko_path: str) -> list[ModuleSymbol]:
    """Minimal ELF symbol table parser for aarch64 .ko files."""
    symbols = []
    try:
        with open(ko_path, "rb") as f:
            # Read ELF header
            ident = f.read(16)
            if ident[:4] != b"\x7fELF":
                return symbols

            ei_class = ident[4]  # 1=32bit, 2=64bit
            ei_data = ident[5]   # 1=LE, 2=BE

            if ei_class != 2:  # Only 64-bit
                return symbols

            endian = "<" if ei_data == 1 else ">"

            # Read rest of ELF header
            f.seek(0)
            ehdr = f.read(64)

            e_shoff = struct.unpack_from(f"{endian}Q", ehdr, 40)[0]
            e_shentsize = struct.unpack_from(f"{endian}H", ehdr, 58)[0]
            e_shnum = struct.unpack_from(f"{endian}H", ehdr, 60)[0]
            e_shstrndx = struct.unpack_from(f"{endian}H", ehdr, 62)[0]

            # Read section headers
            f.seek(e_shoff)
            shdrs = []
            for i in range(e_shnum):
                shdr_data = f.read(e_shentsize)
                sh_name = struct.unpack_from(f"{endian}I", shdr_data, 0)[0]
                sh_type = struct.unpack_from(f"{endian}I", shdr_data, 4)[0]
                sh_offset = struct.unpack_from(f"{endian}Q", shdr_data, 24)[0]
                sh_size = struct.unpack_from(f"{endian}Q", shdr_data, 32)[0]
                sh_link = struct.unpack_from(f"{endian}I", shdr_data, 40)[0]
                sh_entsize = struct.unpack_from(f"{endian}Q", shdr_data, 56)[0]
                shdrs.append({
                    "name_off": sh_name, "type": sh_type,
                    "offset": sh_offset, "size": sh_size,
                    "link": sh_link, "entsize": sh_entsize,
                })

            # Find SHT_SYMTAB (type=2) or SHT_DYNSYM (type=11)
            for shdr in shdrs:
                if shdr["type"] not in (2, 11):
                    continue

                strtab_shdr = shdrs[shdr["link"]]
                f.seek(strtab_shdr["offset"])
                strtab = f.read(strtab_shdr["size"])

                entry_size = shdr["entsize"] or 24  # Elf64_Sym = 24 bytes
                num_entries = shdr["size"] // entry_size

                f.seek(shdr["offset"])
                for _ in range(num_entries):
                    sym_data = f.read(entry_size)
                    if len(sym_data) < 24:
                        break

                    st_name = struct.unpack_from(f"{endian}I", sym_data, 0)[0]
                    st_info = sym_data[4]
                    st_shndx = struct.unpack_from(f"{endian}H", sym_data, 6)[0]

                    # Extract name from string table
                    name_end = strtab.find(b"\x00", st_name)
                    if name_end == -1:
                        name_end = len(strtab)
                    name = strtab[st_name:name_end].decode("ascii", errors="replace")

                    if not name or name.startswith("$"):
                        continue

                    bind = (st_info >> 4)  # 0=LOCAL, 1=GLOBAL, 2=WEAK
                    is_global = bind in (1, 2)
                    is_undefined = (st_shndx == 0)  # SHN_UNDEF

                    symbols.append(ModuleSymbol(
                        name=name,
                        is_undefined=is_undefined,
                        is_global=is_global,
                        section=str(st_shndx),
                    ))
    except Exception as e:
        print(f"Warning: failed to parse {ko_path}: {e}", file=sys.stderr)

    return symbols


def compare_registry_to_symvers(
    registry_syms: set[str],
    symvers: dict[str, SymversEntry],
) -> tuple[set[str], set[str], set[str]]:
    """Compare our registry against GKI symvers.

    Returns (matched, missing_from_registry, extra_in_registry).
    - matched: symbols we provide that are in GKI's export list
    - missing_from_registry: GKI exports we don't provide
    - extra_in_registry: symbols we provide that aren't in GKI's export list
    """
    gki_names = set(symvers.keys())
    matched = registry_syms & gki_names
    missing = gki_names - registry_syms
    extra = registry_syms - gki_names
    return matched, missing, extra


def check_module_symbols(
    module_syms: list[ModuleSymbol],
    registry_syms: set[str],
    symvers: Optional[dict[str, SymversEntry]] = None,
) -> tuple[list[str], list[str], list[str]]:
    """Check which symbols a module needs that we can/can't provide.

    Returns (resolved, unresolved, exports).
    """
    resolved = []
    unresolved = []
    exports = []

    for sym in module_syms:
        if sym.is_undefined and sym.is_global:
            if sym.name in registry_syms:
                resolved.append(sym.name)
            elif symvers and sym.name in symvers:
                # GKI provides it but we don't have a shim
                unresolved.append(sym.name)
            else:
                unresolved.append(sym.name)
        elif not sym.is_undefined and sym.is_global:
            exports.append(sym.name)

    return resolved, unresolved, exports


def generate_stubs(missing: set[str], symvers: dict[str, SymversEntry]) -> str:
    """Generate C stub code for missing symbols."""
    lines = [
        "// Auto-generated stubs for missing GKI symbols.",
        "// Review and implement proper shims where needed.",
        "",
        '#include <cstdio>',
        "",
        'extern "C" {',
        "",
    ]

    for name in sorted(missing):
        entry = symvers.get(name)
        module = entry.module if entry else "unknown"
        export = entry.export_type if entry else "EXPORT_SYMBOL"
        lines.append(f"// From {module} ({export})")
        lines.append(f"void {name}(void) {{")
        lines.append(f'  fprintf(stderr, "driverhub: STUB: {name}() called\\n");')
        lines.append(f"}}")
        lines.append("")

    lines.append("}  // extern \"C\"")
    return "\n".join(lines)


def generate_registry_entries(missing: set[str]) -> str:
    """Generate REGISTER_SYMBOL entries for missing symbols."""
    lines = ["  // Auto-generated: missing GKI symbols"]
    for name in sorted(missing):
        lines.append(f"  REGISTER_SYMBOL({name});")
    return "\n".join(lines)


def parse_kmi_symbol_list(path: str) -> set[str]:
    """Parse a GKI KMI symbol list file.

    KMI symbol lists are plain text files with one symbol name per line.
    Used by the GKI build system to track which exported symbols are part
    of the stable KMI. See:
      https://source.android.com/docs/core/architecture/kernel/howto-symbol-lists

    Format: one symbol name per line, lines starting with # are comments.
    """
    symbols = set()
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            # Some files have format: [group] symbol
            parts = line.split()
            symbols.add(parts[-1])  # Take the last token as the symbol name.
    return symbols


def compare_kmi_to_registry(kmi_symbols: set[str], registry: set[str]) -> tuple[set[str], set[str], set[str]]:
    """Compare KMI symbol list against our registry.

    Returns (matched, missing_from_registry, extra_in_registry).
    """
    matched = kmi_symbols & registry
    missing = kmi_symbols - registry
    extra = registry - kmi_symbols
    return matched, missing, extra


def main():
    parser = argparse.ArgumentParser(
        description="Parse GKI vmlinux.symvers and compare with DriverHub registry"
    )
    parser.add_argument(
        "--symvers", required=False,
        help="Path to vmlinux.symvers or Module.symvers file"
    )
    parser.add_argument(
        "--kmi-symbols",
        help="Path to a GKI KMI symbol list file (plain text, one symbol per line)"
    )
    parser.add_argument(
        "--registry",
        help="Path to symbol_registry.cc to compare against"
    )
    parser.add_argument(
        "--module",
        help="Path to a .ko module to check symbol requirements"
    )
    parser.add_argument(
        "--module-dir",
        help="Directory of .ko modules to bulk-check"
    )
    parser.add_argument(
        "--dump", action="store_true",
        help="Dump all symbols from symvers"
    )
    parser.add_argument(
        "--generate-stubs", action="store_true",
        help="Generate C stub code for missing symbols"
    )
    parser.add_argument(
        "--generate-registry", action="store_true",
        help="Generate REGISTER_SYMBOL entries for missing symbols"
    )
    parser.add_argument(
        "--top", type=int, default=0,
        help="Show only the top N most-needed missing symbols"
    )
    parser.add_argument(
        "--category", action="store_true",
        help="Categorize symbols by subsystem"
    )

    args = parser.parse_args()

    if not args.symvers and not args.kmi_symbols:
        parser.error("Must specify at least one of --symvers or --kmi-symbols")

    # Parse KMI symbol list if provided.
    if args.kmi_symbols:
        print(f"Parsing KMI symbol list {args.kmi_symbols}...")
        kmi_symbols = parse_kmi_symbol_list(args.kmi_symbols)
        print(f"  {len(kmi_symbols)} KMI symbols")

        if args.registry:
            registry = parse_registry(args.registry)
            matched, missing, extra = compare_kmi_to_registry(kmi_symbols, registry)
            coverage = len(matched) / len(kmi_symbols) * 100 if kmi_symbols else 0
            print(f"\n=== KMI Symbol Coverage ===")
            print(f"  KMI symbols:       {len(kmi_symbols)}")
            print(f"  Registry provides: {len(registry)}")
            print(f"  Matched:           {len(matched)} ({coverage:.1f}%)")
            print(f"  Missing (KMI needs, we don't have): {len(missing)}")
            print(f"  Extra (we have, not in KMI): {len(extra)}")

            if missing:
                print(f"\n--- Missing KMI symbols (top {min(50, len(missing))}) ---")
                for name in sorted(missing)[:50]:
                    print(f"  {name}")

        if not args.symvers:
            return

    # Parse symvers
    print(f"Parsing {args.symvers}...")
    symvers = parse_symvers(args.symvers)
    print(f"  {len(symvers)} symbols in symvers")

    vmlinux_syms = {k: v for k, v in symvers.items() if v.module == "vmlinux"}
    module_syms = {k: v for k, v in symvers.items() if v.module != "vmlinux"}
    gpl_syms = {k: v for k, v in symvers.items() if "GPL" in v.export_type}
    print(f"  {len(vmlinux_syms)} from vmlinux, {len(module_syms)} from modules")
    print(f"  {len(gpl_syms)} are EXPORT_SYMBOL_GPL")

    if args.dump:
        print(f"\n{'CRC':>12}  {'Symbol':<50}  {'Module':<30}  {'Type'}")
        print("-" * 110)
        for name in sorted(symvers.keys()):
            e = symvers[name]
            print(f"  0x{e.crc:08x}  {e.name:<50}  {e.module:<30}  {e.export_type}")
        return

    # Compare against registry
    if args.registry:
        print(f"\nParsing {args.registry}...")
        registry = parse_registry(args.registry)
        print(f"  {len(registry)} symbols registered")

        matched, missing, extra = compare_registry_to_symvers(registry, symvers)

        coverage = len(matched) / len(symvers) * 100 if symvers else 0
        print(f"\n=== Symbol Coverage ===")
        print(f"  GKI exports:      {len(symvers)}")
        print(f"  Registry provides: {len(registry)}")
        print(f"  Matched:           {len(matched)} ({coverage:.1f}%)")
        print(f"  Missing (GKI has, we don't): {len(missing)}")
        print(f"  Extra (we have, GKI doesn't): {len(extra)}")

        if missing:
            print(f"\n--- Missing symbols (top {min(50, len(missing))}) ---")
            for name in sorted(missing)[:50]:
                e = symvers[name]
                print(f"  {name:<50}  {e.module:<20}  {e.export_type}")

        if extra and len(extra) < 30:
            print(f"\n--- Extra symbols (not in GKI) ---")
            for name in sorted(extra):
                print(f"  {name}")

        if args.category:
            _print_categories(missing, symvers)

        if args.generate_stubs:
            stub_code = generate_stubs(missing, symvers)
            out_path = "generated_stubs.cc"
            with open(out_path, "w") as f:
                f.write(stub_code)
            print(f"\nGenerated stubs in {out_path}")

        if args.generate_registry:
            reg_code = generate_registry_entries(missing)
            print(f"\n--- REGISTER_SYMBOL entries ---")
            print(reg_code)

    # Check a single module
    if args.module:
        _check_single_module(args.module, parse_registry(args.registry) if args.registry else set(), symvers)

    # Bulk check modules
    if args.module_dir:
        registry = parse_registry(args.registry) if args.registry else set()
        _check_module_dir(args.module_dir, registry, symvers, top=args.top)


def _check_single_module(ko_path: str, registry: set[str], symvers: dict[str, SymversEntry]):
    """Check symbol requirements for a single .ko module."""
    print(f"\nAnalyzing {ko_path}...")
    mod_syms = parse_module_symbols(ko_path)

    undefined = [s for s in mod_syms if s.is_undefined and s.is_global]
    defined = [s for s in mod_syms if not s.is_undefined and s.is_global]

    print(f"  Undefined (needs):  {len(undefined)}")
    print(f"  Defined (exports):  {len(defined)}")

    resolved, unresolved, exports = check_module_symbols(mod_syms, registry, symvers)

    print(f"  Resolved by registry: {len(resolved)}")
    print(f"  Unresolved:           {len(unresolved)}")

    if unresolved:
        print(f"\n  --- Unresolved symbols ---")
        for name in sorted(unresolved):
            in_gki = "  (in GKI)" if name in symvers else "  (NOT in GKI!)"
            print(f"    {name}{in_gki}")

    if exports:
        print(f"\n  --- Exports ({len(exports)}) ---")
        for name in sorted(exports)[:20]:
            print(f"    {name}")
        if len(exports) > 20:
            print(f"    ... and {len(exports) - 20} more")


def _check_module_dir(
    dir_path: str,
    registry: set[str],
    symvers: dict[str, SymversEntry],
    top: int = 0,
):
    """Bulk check all .ko modules in a directory."""
    ko_files = sorted(Path(dir_path).rglob("*.ko"))
    if not ko_files:
        print(f"No .ko files found in {dir_path}")
        return

    print(f"\n=== Bulk Module Analysis: {len(ko_files)} modules ===\n")

    # Track per-symbol frequency of being needed
    symbol_demand: dict[str, int] = defaultdict(int)
    module_results = []

    for ko in ko_files:
        mod_syms = parse_module_symbols(str(ko))
        resolved, unresolved, exports = check_module_symbols(
            mod_syms, registry, symvers
        )
        total_needed = len(resolved) + len(unresolved)
        pct = len(resolved) / total_needed * 100 if total_needed else 100

        module_results.append({
            "name": ko.name,
            "resolved": len(resolved),
            "unresolved": len(unresolved),
            "exports": len(exports),
            "pct": pct,
            "missing": unresolved,
        })

        for sym in unresolved:
            symbol_demand[sym] += 1

    # Sort by coverage
    module_results.sort(key=lambda r: r["pct"], reverse=True)

    # Print per-module summary
    fully_loadable = sum(1 for r in module_results if r["unresolved"] == 0)
    print(f"{'Module':<45} {'Resolved':>8} {'Missing':>8} {'Coverage':>8}")
    print("-" * 75)
    for r in module_results:
        status = "OK" if r["unresolved"] == 0 else f"{r['unresolved']} missing"
        print(f"  {r['name']:<43} {r['resolved']:>8} {r['unresolved']:>8} {r['pct']:>7.1f}%")

    print(f"\n  Fully loadable: {fully_loadable}/{len(module_results)}")

    # Print most-demanded missing symbols
    if symbol_demand:
        sorted_demand = sorted(symbol_demand.items(), key=lambda x: -x[1])
        limit = top if top > 0 else 30
        print(f"\n--- Most-needed missing symbols (top {min(limit, len(sorted_demand))}) ---")
        print(f"  {'Symbol':<50} {'Modules needing it':>20}")
        print("  " + "-" * 72)
        for sym, count in sorted_demand[:limit]:
            in_gki = "(GKI)" if sym in symvers else "(custom)"
            print(f"  {sym:<50} {count:>8}  {in_gki}")


def _print_categories(missing: set[str], symvers: dict[str, SymversEntry]):
    """Categorize missing symbols by subsystem."""
    categories: dict[str, list[str]] = defaultdict(list)

    # Simple prefix-based categorization
    prefixes = {
        "mutex_": "sync", "spin_": "sync", "__mutex": "sync",
        "kmalloc": "memory", "kfree": "memory", "kzalloc": "memory",
        "vmalloc": "memory", "krealloc": "memory",
        "printk": "print", "_printk": "print", "dev_": "print",
        "device_": "device", "driver_": "device", "bus_": "device",
        "class_": "device",
        "irq_": "irq", "request_irq": "irq", "free_irq": "irq",
        "dma_": "dma",
        "clk_": "clock", "devm_clk": "clock",
        "gpio_": "gpio", "gpiod_": "gpio", "gpiochip_": "gpio",
        "i2c_": "i2c",
        "spi_": "spi",
        "usb_": "usb",
        "regulator_": "regulator",
        "pm_runtime_": "power",
        "of_": "devicetree",
        "platform_": "platform",
        "pci_": "pci",
        "input_": "input",
        "iio_": "iio",
        "thermal_": "thermal",
        "pwm_": "pwm",
        "led_": "led",
        "sysfs_": "sysfs", "kobject_": "sysfs",
        "proc_": "procfs",
        "debugfs_": "debugfs",
        "seq_": "seqfile",
        "cdev_": "chardev",
        "misc_": "miscdev",
        "net": "network", "sk_": "network", "skb_": "network",
        "scsi_": "scsi",
        "blk_": "block",
        "drm_": "drm",
        "regmap_": "regmap",
        "iommu_": "iommu",
        "pinctrl_": "pinctrl",
        "nvmem_": "nvmem",
        "power_supply_": "power_supply",
        "watchdog_": "watchdog",
        "rtc_": "rtc",
        "crypto_": "crypto",
        "firmware_": "firmware",
        "workqueue": "workqueue", "queue_work": "workqueue",
        "schedule": "scheduler", "__schedule": "scheduler",
        "timer_": "timer", "mod_timer": "timer", "add_timer": "timer",
        "wait_": "waitqueue", "wake_up": "waitqueue",
        "completion_": "completion",
        "kthread_": "kthread",
        "rcu_": "rcu",
        "__": "internal",
    }

    for sym in missing:
        categorized = False
        for prefix, cat in prefixes.items():
            if sym.startswith(prefix):
                categories[cat].append(sym)
                categorized = True
                break
        if not categorized:
            categories["other"].append(sym)

    print(f"\n--- Missing symbols by category ---")
    for cat in sorted(categories.keys(), key=lambda c: -len(categories[c])):
        syms = categories[cat]
        print(f"\n  {cat} ({len(syms)}):")
        for s in sorted(syms)[:10]:
            print(f"    {s}")
        if len(syms) > 10:
            print(f"    ... and {len(syms) - 10} more")


if __name__ == "__main__":
    main()
