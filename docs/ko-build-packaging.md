# Building and Packaging .ko Modules for DriverHub

This guide explains how to build Linux GKI `.ko` kernel modules and package
them as Fuchsia components for use with DriverHub.

## Overview

DriverHub loads unmodified ARM64 (or x86_64) Linux `.ko` files — ELF
relocatable objects (`ET_REL`). These are the same `.ko` binaries produced by
the GKI kernel build. You do **not** need to recompile modules against
DriverHub; they are loaded as-is and symbols are resolved against the KMI
shim library at runtime.

```
Source .ko (from GKI build or vendor)
  → Package as Fuchsia component resource
    → DriverHub runner loads via elfldltl
      → Symbols resolved against libdriverhub_kmi.so
```

## 1. Obtaining .ko Modules

### From a GKI kernel build

Build the GKI kernel with modules enabled:

```shell
# Clone the Android kernel source
repo init -u https://android.googlesource.com/kernel/manifest \
  -b common-android15-6.6
repo sync -j$(nproc)

# Build GKI + modules
BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh

# Modules are in out/android15-6.6/dist/*.ko
ls out/android15-6.6/dist/*.ko
```

### From a factory image

Use the `extract-factory` tool to pull `.ko` files from a Pixel factory image:

```shell
cd tools/extract-factory
cargo build --release

./target/release/extract-factory \
  --zip /path/to/factory.zip \
  --output /tmp/pixel-modules \
  --list-symbols
```

### From a vendor kernel tree

Vendor-specific modules (e.g., Qualcomm, MediaTek, Samsung) are built as
part of the vendor kernel. Cross-compile for ARM64:

```shell
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export KERNEL_SRC=/path/to/gki-kernel

make -C $KERNEL_SRC M=/path/to/vendor/module modules
# Output: /path/to/vendor/module/*.ko
```

## 2. Verifying Module Compatibility

Before packaging, verify that DriverHub can resolve the module's symbols.

### Check symbol coverage

```shell
# Compare a single module against the DriverHub symbol registry
./tools/symvers_parser.py \
  --symvers /path/to/vmlinux.symvers \
  --registry src/symbols/symbol_registry.cc \
  --module /path/to/my_module.ko

# Bulk-check all modules in a directory
./tools/symvers_parser.py \
  --symvers /path/to/vmlinux.symvers \
  --registry src/symbols/symbol_registry.cc \
  --module-dir /path/to/modules/
```

### Interpret the output

```
Analyzing my_module.ko...
  Undefined (needs):  42
  Defined (exports):   5
  Resolved by registry: 38
  Unresolved:            4

  --- Unresolved symbols ---
    some_missing_func  (in GKI)
    another_func       (NOT in GKI!)
```

- **Resolved**: DriverHub's KMI shim provides this symbol. Module will load.
- **Unresolved (in GKI)**: The symbol exists in the real kernel but DriverHub
  doesn't have a shim yet. You need to add a shim or stub.
- **Unresolved (NOT in GKI!)**: The symbol comes from another `.ko` module.
  Load the dependency module first, or colocate them.

### Generate stubs for missing symbols

```shell
./tools/generate_stubs.py \
  --headers /path/to/gki-headers/include \
  --symvers /path/to/vmlinux.symvers \
  --registry src/symbols/symbol_registry.cc \
  --modules /path/to/modules/ \
  --output-stubs src/shim/kernel/new_stubs.cc \
  --output-registry /tmp/register_entries.txt
```

## 3. Packaging with GN (Fuchsia In-Tree Build)

### The ko_module() GN template

DriverHub provides a `ko_module()` template that wraps `fuchsia_component()`:

```gn
# In BUILD.gn:
import("//src/drivers/driverhub/ko_module.gni")

ko_module("bmp280") {
  ko_binary = "prebuilt/bmp280.ko"
  manifest = "meta/bmp280.cml"
}
```

This:
1. Creates a Fuchsia component named `bmp280`
2. Bundles the `.ko` file at `data/modules/bmp280.ko` inside the package
3. Uses the specified `.cml` manifest

### Writing the component manifest (.cml)

Each `.ko` module needs a `.cml` manifest that declares `runner: "ko"` and
the FIDL capabilities the module requires.

#### Minimal manifest (standalone module)

```json5
// meta/bmp280.cml
{
    include: [
        "syslog/client.shard.cml",
    ],
    program: {
        runner: "ko",
        binary: "data/modules/bmp280.ko",
    },
    use: [
        {
            protocol: "fuchsia.driverhub.SymbolRegistry",
        },
        {
            service: "fuchsia.hardware.i2c.Service",
        },
    ],
}
```

#### Colocated modules (shared process)

For tightly-coupled modules that share many symbols (e.g., WiFi stack):

```json5
// meta/iwlwifi.cml
{
    include: [
        "syslog/client.shard.cml",
    ],
    program: {
        runner: "ko",
        binary: "data/modules/iwlwifi.ko",
        colocation_group: "wifi",
        group_modules: [
            "data/modules/cfg80211.ko",
            "data/modules/iwlwifi.ko",
        ],
    },
    use: [
        {
            protocol: "fuchsia.driverhub.SymbolRegistry",
        },
        {
            protocol: "fuchsia.hardware.wlan.Service",
        },
    ],
}
```

All modules in the same `colocation_group` share a process. Symbols resolve
directly (no FIDL proxy overhead). Load order within the group is
determined by dependency analysis.

### Packaging multiple modules together

```gn
# //vendor/my_drivers/BUILD.gn

import("//src/drivers/driverhub/ko_module.gni")

ko_module("bmp280") {
  ko_binary = "prebuilt/bmp280.ko"
  manifest = "meta/bmp280.cml"
}

ko_module("cfg80211") {
  ko_binary = "prebuilt/cfg80211.ko"
  manifest = "meta/cfg80211.cml"
}

ko_module("iwlwifi") {
  ko_binary = "prebuilt/iwlwifi.ko"
  manifest = "meta/iwlwifi.cml"
  deps = [ ":cfg80211" ]  # Build-time dependency tracking
}

fuchsia_package("my-ko-drivers") {
  deps = [
    ":bmp280",
    ":cfg80211",
    ":iwlwifi",
  ]
}
```

### Using ko_module.gni

To use the template from another GN file, import it:

```gn
import("//src/drivers/driverhub/ko_module.gni")
```

The template definition (from `BUILD.gn`):

```gn
template("ko_module") {
  fuchsia_component(target_name) {
    component_name = target_name
    manifest = invoker.manifest
    deps = []
    if (defined(invoker.deps)) {
      deps += invoker.deps
    }
    resources = [
      {
        path = invoker.ko_binary
        dest = "data/modules/" + get_path_info(invoker.ko_binary, "file")
      },
    ]
  }
}
```

## 4. Packaging with Bazel (Fuchsia Out-of-Tree / SDK Build)

For teams using the Fuchsia SDK with Bazel (`rules_fuchsia`):

### WORKSPACE setup

```python
# WORKSPACE.bazel
load("@rules_fuchsia//fuchsia:deps.bzl", "fuchsia_sdk_repository")

fuchsia_sdk_repository(
    name = "fuchsia_sdk",
    # ...
)
```

### Define a ko_module rule

Create a Bazel macro that mirrors the GN template:

```python
# build_defs/ko_module.bzl

load("@rules_fuchsia//fuchsia:defs.bzl", "fuchsia_component", "fuchsia_package")

def ko_module(name, ko_binary, manifest, deps = [], **kwargs):
    """Package a .ko kernel module as a Fuchsia component.

    Args:
        name: Component name.
        ko_binary: Label or path to the .ko file.
        manifest: Label or path to the .cml manifest.
        deps: Additional dependencies.
    """

    # The .ko binary is bundled as a resource.
    resource_name = name + "_ko_resource"
    native.filegroup(
        name = resource_name,
        srcs = [ko_binary],
    )

    fuchsia_component(
        name = name,
        manifest = manifest,
        resources = [
            {
                "src": ko_binary,
                "dest": "data/modules/" + native.package_relative_label(ko_binary).name,
            },
        ],
        deps = deps,
        **kwargs,
    )
```

### Usage in BUILD.bazel

```python
# //vendor/drivers/BUILD.bazel

load("//build_defs:ko_module.bzl", "ko_module")
load("@rules_fuchsia//fuchsia:defs.bzl", "fuchsia_package")

ko_module(
    name = "bmp280",
    ko_binary = "prebuilt/bmp280.ko",
    manifest = "meta/bmp280.cml",
)

ko_module(
    name = "iwlwifi",
    ko_binary = "prebuilt/iwlwifi.ko",
    manifest = "meta/iwlwifi.cml",
    deps = [":cfg80211"],
)

fuchsia_package(
    name = "my_ko_drivers",
    components = [
        ":bmp280",
        ":iwlwifi",
    ],
)
```

### Building and publishing

```shell
# Build the package
bazel build //vendor/drivers:my_ko_drivers

# Publish to a Fuchsia package repository
bazel run //vendor/drivers:my_ko_drivers.publish
```

## 5. Cross-Compiling .ko Modules for Testing

For development, you may want to build minimal test `.ko` modules. These
are `ET_REL` ELF objects — just compile to `.o`:

### For x86_64 host testing

```shell
gcc -c -o my_module.ko my_module.c \
  -Isrc/shim/include \
  -fno-stack-protector \
  -fno-pie
```

### For ARM64 (GKI target)

```shell
aarch64-linux-gnu-gcc -c -o my_module.ko my_module.c \
  -Isrc/shim/include \
  -fno-stack-protector \
  -fno-pie \
  -mcmodel=large
```

### Test module template

```c
// my_module.c
#include <linux/module.h>
#include <linux/init.h>

static int __init my_module_init(void) {
    printk(KERN_INFO "my_module: loaded\n");
    return 0;
}

static void __exit my_module_exit(void) {
    printk(KERN_INFO "my_module: unloaded\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Test module for DriverHub");
```

## 6. Bootfs Deployment (Early Boot)

For modules needed before the package resolver, use ZBI bootfs:

```shell
ZBI=third_party/fuchsia-idk/tools/x64/zbi

$ZBI -o my-image.zbi \
  base-image.zbi \
  --entry data/modules/early_driver.ko=prebuilt/early_driver.ko
```

The runner checks `/boot/data/modules/` as a fallback when `/pkg/` paths
are not available.

## 7. Module Dependencies

### Declaring dependencies

Module dependencies come from two sources:

1. **modinfo `depends=` field**: Embedded in the `.ko` by the kernel build
   system. DriverHub parses this and loads dependencies first.

2. **Colocation groups**: Declared in `.cml` manifests via
   `colocation_group` and `group_modules`.

### Dependency resolution order

```
1. Parse .modinfo from each .ko
2. Build dependency graph from depends= fields
3. Topological sort (dependency_sort.cc)
4. Load in sorted order: dependencies before dependents
5. Within a colocation group: dependencies loaded first in same process
```

### Checking dependencies

```shell
# View a module's modinfo (including depends=)
modinfo /path/to/module.ko

# Or use DriverHub's parser:
./driverhub --info /path/to/module.ko
```

## 8. Quick Reference

### File layout for a ko_module

```
my-ko-package/
├── BUILD.gn          # ko_module() target + fuchsia_package()
├── meta/
│   └── my_module.cml # Component manifest (runner: "ko")
└── prebuilt/
    └── my_module.ko  # The .ko binary (not checked into git)
```

### .cml program block fields

| Field | Required | Description |
|-------|----------|-------------|
| `runner` | Yes | Must be `"ko"` |
| `binary` | Yes | Path to .ko in package (`data/modules/foo.ko`) |
| `colocation_group` | No | Group name for shared-process modules |
| `group_modules` | No | Ordered list of all .ko paths in the group |

### GN template parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `ko_binary` | string | Host path to the `.ko` file |
| `manifest` | string | Path to the `.cml` manifest |
| `deps` | list | Additional GN dependencies |
