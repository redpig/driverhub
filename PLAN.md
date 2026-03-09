# Plan: GKI Symbol Map Parser + Pixel Factory Image Extraction

## 1. Symbol Map Parser (`tools/symvers_parser.py`)
- Parse `vmlinux.symvers` (format: `CRC\tsymbol\tnamespace\tmodule\texport_type`)
- Compare against our SymbolRegistry to detect missing/extra symbols
- Report CRC mismatches when modversions is enabled
- Generate stub suggestions for missing symbols

## 2. Factory Image Extraction Script (`tools/extract-pixel-factory.sh`)
- Download + extract Pixel factory ZIP
- Extract vendor_boot.img → DTB (via unpack_bootimg)
- Extract super.img → vendor_dlkm.img → .ko module files
- Also extract system_dlkm for system modules

## 3. Module Loading Test Script (`tools/test-all-modules.sh`)
- For each .ko from the factory image, attempt to load with ModuleLoader
- Report unresolved symbols per module
- Track success/failure statistics

## 4. C++ SymbolRegistry integration
- Add `LoadSymvers()` method to compare registry against vmlinux.symvers
- Report coverage percentage
