#!/usr/bin/env python3
# Copyright 2024 The DriverHub Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate KMI stub implementations from GKI kernel headers.

Scans GKI kernel headers for function declarations matching missing symbols,
then generates stub .c files with no-op/zero-return implementations and
REGISTER_SYMBOL entries for the symbol registry.

Usage:
  ./tools/generate_stubs.py \
    --headers third_party/gki-headers/include \
    --symvers /tmp/gki-artifacts/vmlinux.symvers \
    --registry src/symbols/symbol_registry.cc \
    --modules /tmp/gki-artifacts/modules/flatten/lib/modules \
    --output-stubs src/shim/kernel/gki_stubs.cc \
    --output-registry /tmp/register_entries.txt
"""

import argparse
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path


def get_registered_symbols(registry_path: str) -> set[str]:
    """Parse symbol_registry.cc for already-registered symbols."""
    with open(registry_path) as f:
        content = f.read()
    symbols = set(re.findall(r'REGISTER_SYMBOL\((\w+)\)', content))
    symbols.update(re.findall(r'Register\("(\w+)"', content))
    return symbols


def get_module_undefined_symbols(module_dir: str) -> Counter:
    """Get all undefined symbols across all .ko modules."""
    sym_count = Counter()
    for name in sorted(os.listdir(module_dir)):
        if not name.endswith('.ko'):
            continue
        path = os.path.join(module_dir, name)
        try:
            result = subprocess.run(
                ['readelf', '-s', path],
                capture_output=True, text=True, timeout=10
            )
            for line in result.stdout.splitlines():
                parts = line.split()
                if (len(parts) >= 8 and parts[4] == 'GLOBAL' and
                        parts[6] == 'UND' and parts[7]):
                    sym_count[parts[7]] += 1
        except Exception:
            pass
    return sym_count


def find_symbol_in_headers(symbol: str, headers_dir: str) -> list[dict]:
    """Search for a symbol's declaration in kernel headers.

    Returns list of matches with file, line, and declaration text.
    """
    matches = []

    # Use grep to find the symbol in headers
    try:
        result = subprocess.run(
            ['grep', '-rn', f'\\b{symbol}\\b', headers_dir],
            capture_output=True, text=True, timeout=10
        )
    except Exception:
        return matches

    for line in result.stdout.splitlines():
        # Skip comments and #define lines that just mention the name
        if '//' in line.split(symbol)[0] or '* ' in line.split(symbol)[0]:
            continue

        # Look for function declarations
        # Pattern: return_type symbol_name(args);
        # or: extern type symbol_name;
        decl_match = re.search(
            rf'(?:extern\s+)?[\w\s*]+\b{re.escape(symbol)}\s*\([^)]*\)\s*;',
            line
        )
        if decl_match:
            parts = line.split(':', 2)
            if len(parts) >= 3:
                matches.append({
                    'file': parts[0],
                    'line': int(parts[1]),
                    'decl': decl_match.group(0).strip(),
                    'type': 'function'
                })
            continue

        # Look for extern variable declarations
        var_match = re.search(
            rf'extern\s+[\w\s*]+\b{re.escape(symbol)}\s*(?:\[[\w\s]*\])?\s*;',
            line
        )
        if var_match:
            parts = line.split(':', 2)
            if len(parts) >= 3:
                matches.append({
                    'file': parts[0],
                    'line': int(parts[1]),
                    'decl': var_match.group(0).strip(),
                    'type': 'variable'
                })

    return matches


def generate_stub_implementation(symbol: str, decl: str) -> str:
    """Generate a stub implementation for a function declaration."""
    # Clean up the declaration
    decl = decl.strip().rstrip(';').strip()
    decl = re.sub(r'\bextern\b\s*', '', decl)
    decl = re.sub(r'\b__printf\s*\([^)]*\)', '', decl)
    decl = re.sub(r'\b__scanf\s*\([^)]*\)', '', decl)
    decl = re.sub(r'\b__attribute__\s*\(\([^)]*\)\)', '', decl)
    decl = re.sub(r'\b__must_check\b', '', decl)
    decl = re.sub(r'\b__cold\b', '', decl)
    decl = re.sub(r'\b__always_inline\b', '', decl)
    decl = re.sub(r'\bnoinline\b', '', decl)
    decl = re.sub(r'\b__init\b', '', decl)
    decl = re.sub(r'\b__exit\b', '', decl)
    decl = re.sub(r'\b__weak\b', '', decl)
    decl = re.sub(r'\basmlinkage\b', '', decl)
    decl = re.sub(r'\bnotrace\b', '', decl)
    decl = decl.strip()

    # Determine return type
    ret_type = 'void'
    match = re.match(r'([\w\s*]+?)\s+\*?' + re.escape(symbol), decl)
    if match:
        ret_type = match.group(1).strip()

    # Check if it returns a pointer
    is_ptr = '*' in decl.split('(')[0].split(symbol)[0] if '(' in decl else False
    returns_void = ret_type == 'void' and not is_ptr

    # Generate the body
    if returns_void:
        body = '  /* stub */\n'
    elif is_ptr:
        body = '  return NULL;\n'
    elif ret_type in ('int', 'long', 'short', 'ssize_t', 'int32_t', 'int64_t',
                       's32', 's64', 'unsigned int', 'unsigned long',
                       'unsigned short', 'size_t', 'uint32_t', 'uint64_t',
                       'u32', 'u64', 'u16', 'u8', 'bool', '_Bool',
                       'loff_t', 'off_t', 'dev_t', 'gfp_t'):
        body = '  return 0;\n'
    elif 'enum' in ret_type:
        body = '  return 0;\n'
    else:
        body = '  return 0;\n'

    # Name the parameters if they're unnamed
    # (kernel headers sometimes have unnamed params)
    # Just use the declaration as-is for the signature

    return f'{decl} {{\n{body}}}\n'


def generate_variable_stub(symbol: str, decl: str) -> str:
    """Generate a stub definition for an extern variable."""
    decl = decl.strip().rstrip(';').strip()
    decl = re.sub(r'\bextern\b\s*', '', decl).strip()
    # Initialize to zero
    return f'{decl} = {{0}};\n'


def main():
    parser = argparse.ArgumentParser(
        description='Generate KMI stub implementations from GKI kernel headers'
    )
    parser.add_argument('--headers', required=True,
                        help='Path to GKI kernel headers include directory')
    parser.add_argument('--symvers',
                        help='Path to vmlinux.symvers')
    parser.add_argument('--registry', required=True,
                        help='Path to symbol_registry.cc')
    parser.add_argument('--modules', required=True,
                        help='Directory of .ko modules to check')
    parser.add_argument('--output-stubs', default='gki_stubs.cc',
                        help='Output file for stub implementations')
    parser.add_argument('--output-registry', default='register_entries.txt',
                        help='Output file for REGISTER_SYMBOL entries')
    parser.add_argument('--min-modules', type=int, default=1,
                        help='Minimum number of modules needing a symbol to stub it')
    args = parser.parse_args()

    print("Scanning registered symbols...")
    registered = get_registered_symbols(args.registry)
    print(f"  {len(registered)} already registered")

    print("Scanning module undefined symbols...")
    sym_count = get_module_undefined_symbols(args.modules)
    missing = {s: c for s, c in sym_count.items()
               if s not in registered and c >= args.min_modules}
    print(f"  {len(missing)} missing symbols (>= {args.min_modules} modules)")

    print("Searching GKI headers for declarations...")
    found = {}
    not_found = []
    for i, (sym, count) in enumerate(sorted(missing.items(),
                                             key=lambda x: -x[1])):
        if i % 100 == 0:
            print(f"  ... {i}/{len(missing)}")
        matches = find_symbol_in_headers(sym, args.headers)
        if matches:
            # Take the best match (prefer function declarations)
            func_matches = [m for m in matches if m['type'] == 'function']
            if func_matches:
                found[sym] = func_matches[0]
            else:
                found[sym] = matches[0]
        else:
            not_found.append((sym, count))

    print(f"\n  Found declarations: {len(found)}")
    print(f"  Not found: {len(not_found)}")

    # Generate stubs
    print(f"\nGenerating stubs -> {args.output_stubs}")
    stubs = []
    stubs.append('// Auto-generated KMI stub implementations.\n')
    stubs.append('// Generated from GKI android15-6.6 kernel headers.\n')
    stubs.append('// DO NOT EDIT — regenerate with tools/generate_stubs.py\n\n')
    stubs.append('#include <stddef.h>\n')
    stubs.append('#include <stdint.h>\n')
    stubs.append('#include <string.h>\n\n')
    stubs.append('#ifdef __cplusplus\n')
    stubs.append('extern "C" {\n')
    stubs.append('#endif\n\n')

    # Add typedefs for kernel types not available on host
    stubs.append('// Kernel type stubs for host compilation.\n')
    stubs.append('typedef unsigned int gfp_t;\n')
    stubs.append('typedef long long loff_t;\n')
    stubs.append('typedef unsigned char u8;\n')
    stubs.append('typedef unsigned short u16;\n')
    stubs.append('typedef unsigned int u32;\n')
    stubs.append('typedef unsigned long long u64;\n')
    stubs.append('typedef signed char s8;\n')
    stubs.append('typedef signed short s16;\n')
    stubs.append('typedef signed int s32;\n')
    stubs.append('typedef signed long long s64;\n')
    stubs.append('typedef int bool;\n')
    stubs.append('typedef unsigned long phys_addr_t;\n')
    stubs.append('typedef unsigned long dma_addr_t;\n')
    stubs.append('typedef unsigned int dev_t;\n')
    stubs.append('typedef int atomic_t;\n')
    stubs.append('typedef unsigned int fmode_t;\n')
    stubs.append('#define __user\n')
    stubs.append('#define __iomem\n')
    stubs.append('#define __force\n')
    stubs.append('#define __bitwise\n\n')

    # Generate for found symbols
    func_count = 0
    var_count = 0
    for sym in sorted(found.keys()):
        info = found[sym]
        count = missing[sym]
        header_rel = info['file'].replace(args.headers + '/', '')
        stubs.append(f'// {sym} — needed by {count} module(s)\n')
        stubs.append(f'// Source: {header_rel}:{info["line"]}\n')
        if info['type'] == 'function':
            stubs.append(generate_stub_implementation(sym, info['decl']))
            func_count += 1
        else:
            stubs.append(generate_variable_stub(sym, info['decl']))
            var_count += 1
        stubs.append('\n')

    # Generate blind stubs for symbols not found in headers
    stubs.append('\n// ---- Symbols not found in kernel headers ----\n')
    stubs.append('// These may be compiler builtins, inline functions,\n')
    stubs.append('// or symbols from modules (not vmlinux).\n\n')
    for sym, count in sorted(not_found, key=lambda x: -x[1]):
        stubs.append(f'// {sym} — needed by {count} module(s) [no header found]\n')
        stubs.append(f'void {sym}(void) {{ /* blind stub */ }}\n\n')

    stubs.append('#ifdef __cplusplus\n')
    stubs.append('}  // extern "C"\n')
    stubs.append('#endif\n')

    with open(args.output_stubs, 'w') as f:
        f.writelines(stubs)

    # Generate REGISTER_SYMBOL entries
    print(f"Generating registry entries -> {args.output_registry}")
    reg_entries = []
    all_syms = list(found.keys()) + [s for s, _ in not_found]
    for sym in sorted(all_syms):
        reg_entries.append(f'  REGISTER_SYMBOL({sym});\n')

    with open(args.output_registry, 'w') as f:
        f.writelines(reg_entries)

    print(f"\nSummary:")
    print(f"  Function stubs: {func_count}")
    print(f"  Variable stubs: {var_count}")
    print(f"  Blind stubs (no header): {len(not_found)}")
    print(f"  Total REGISTER_SYMBOL entries: {len(all_syms)}")


if __name__ == '__main__':
    main()
