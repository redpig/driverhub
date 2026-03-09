# GKI Kernel Headers

These headers are extracted from the GKI (Generic Kernel Image) CI build
artifacts for use by DriverHub's KMI shim layer.

## Version Info

- **Kernel branch**: `android15-6.6`
- **Release tag**: `android15-6.6-2026-01_r15`
- **CI build ID**: `14943902`
- **Download date**: 2026-03-09
- **Artifacts URL**: https://ci.android.com/builds/submitted/14943902/kernel_aarch64/latest
- **Architecture**: aarch64

## How to Update

1. Find the latest release build at:
   https://source.android.com/docs/core/architecture/kernel/gki-android15-6_6-release-builds

2. Download the new `kernel-headers.tar.gz`:
   ```sh
   BUILD_ID=<new_build_id>
   curl -L -o kernel-headers.tar.gz \
     "https://ci.android.com/builds/submitted/${BUILD_ID}/kernel_aarch64/latest/raw/kernel-headers.tar.gz"
   ```

3. Extract and replace:
   ```sh
   rm -rf third_party/gki-headers/include third_party/gki-headers/arch
   tar xzf kernel-headers.tar.gz
   cp -r kernel-headers/include third_party/gki-headers/
   cp -r kernel-headers/arch third_party/gki-headers/
   ```

4. Update the version info in this README.

5. Re-run the symbol coverage analysis:
   ```sh
   curl -L -o vmlinux.symvers \
     "https://ci.android.com/builds/submitted/${BUILD_ID}/kernel_aarch64/latest/raw/vmlinux.symvers"
   python3 tools/symvers_parser.py --symvers vmlinux.symvers \
     --registry src/symbols/symbol_registry.cc --category
   ```

6. Check for struct size changes (ANDROID_KABI_USE/REPLACE):
   ```sh
   # Compare struct sizes between old and new headers
   diff <(grep -r 'ANDROID_KABI_USE\|ANDROID_KABI_REPLACE' old_headers/) \
        <(grep -r 'ANDROID_KABI_USE\|ANDROID_KABI_REPLACE' third_party/gki-headers/include/)
   ```

## Contents

- `include/` — Kernel headers (`linux/`, `net/`, `asm-generic/`, etc.)
- `arch/` — Architecture-specific headers (arm64)

## License

These headers are from the Android Common Kernel and are licensed under
GPL-2.0 with the Linux-syscall-note exception for UAPI headers.
