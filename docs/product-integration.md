# Integrating DriverHub into a Fuchsia Product Build

This guide covers how to include DriverHub in a Fuchsia product image so that
`.ko` modules can be loaded at runtime via the DriverHub runner.

## Prerequisites

- A working Fuchsia checkout (`jiri` or `repo`-based)
- DriverHub source at `//src/drivers/driverhub` (or your vendor path)
- Target board definition (e.g., `vim3`, `generic-arm64`, `x64`)

## 1. Add DriverHub to Your Product Definition

### Option A: In-tree product (GN)

Add DriverHub to your product's `BUILD.gn` or product config `.gni`:

```gn
# //products/my_product.gni (or board-level BUILD.gn)

# Include the DriverHub runner and the legacy bus driver component.
product_packages += [
  "//src/drivers/driverhub",
]
```

This pulls in the top-level `group("driverhub")` which includes:
- `driverhub-component` — the DFv2 bus driver (legacy model)
- `driverhub-runner-component` — the component runner (new model)
- `driverhub-tests` — unit test package

### Option B: Out-of-tree / vendor overlay

If DriverHub lives in a vendor repository, add it to your product assembly
input bundles:

```json5
// assembly/my_product.json
{
    "platform": { ... },
    "product": {
        "packages": {
            "base": [
                {
                    "manifest": "//vendor/driverhub/meta/driverhub-runner.cm",
                }
            ]
        }
    }
}
```

### fx set

For quick iteration, use `--with`:

```shell
fx set <product>.<board> \
  --with //src/drivers/driverhub \
  --with //src/drivers/driverhub:driverhub-tests

fx build
```

## 2. Register the Runner with Component Framework

The component framework needs to know that `ko` is a valid runner name.
DriverHub's runner component exposes the `ko` runner capability.

### Core realm routing

Add the runner to the driver framework's realm (typically in the platform
assembly or board driver configuration):

```json5
// In your platform assembly or core realm shard:
{
    children: [
        {
            name: "driverhub-runner",
            url: "fuchsia-pkg://fuchsia.com/driverhub#meta/driverhub-runner.cm",
            startup: "eager",
        },
    ],
    offer: [
        {
            runner: "ko",
            from: "#driverhub-runner",
            to: [ "#driver_manager" ],
        },
        {
            protocol: "fuchsia.kernel.VmexResource",
            from: "parent",
            to: [ "#driverhub-runner" ],
        },
        {
            protocol: "fuchsia.process.Launcher",
            from: "parent",
            to: [ "#driverhub-runner" ],
        },
        {
            protocol: "fuchsia.kernel.RootJobForInspect",
            from: "parent",
            to: [ "#driverhub-runner" ],
            availability: "optional",
        },
        {
            // Route DeviceFs to Starnix for procfs/sysfs bridging.
            protocol: "fuchsia.driverhub.DeviceFs",
            from: "#driverhub-runner",
            to: [ "#starnix_manager" ],
        },
        {
            // Route FirmwareLoader from Starnix or firmware service.
            protocol: "fuchsia.driverhub.FirmwareLoader",
            from: "#firmware-service",
            to: [ "#driverhub-runner" ],
            availability: "optional",
        },
    ],
}
```

### Driver manager awareness

The driver manager must be told that `ko` runner components exist.
This is automatic in Fuchsia's `DriverRunner` — it discovers runners
through capability routing. No code changes needed as long as the
`runner: "ko"` capability is offered to `#driver_manager`.

## 3. Include .ko Modules in the Product

### Package-based deployment (recommended)

Each `.ko` module is a Fuchsia component in its own package. Include them
in your product's base or universe package set:

```gn
# //vendor/my_modules/BUILD.gn

import("//src/drivers/driverhub/ko_module.gni")

ko_module("bmp280") {
  ko_binary = "prebuilt/bmp280.ko"
  manifest = "meta/bmp280.cml"
}

ko_module("iwlwifi") {
  ko_binary = "prebuilt/iwlwifi.ko"
  manifest = "meta/iwlwifi.cml"
}

fuchsia_package("my-ko-modules") {
  deps = [
    ":bmp280",
    ":iwlwifi",
  ]
}
```

Then add to the product:

```gn
product_packages += [
  "//vendor/my_modules:my-ko-modules",
]
```

### Bootfs deployment (early boot / bringup)

For drivers needed before the package resolver is available (e.g., storage
or network drivers), bundle into bootfs via the ZBI:

```shell
# Build the runner and KMI shim
fx build //src/drivers/driverhub:driverhub_runner
fx build //src/drivers/driverhub:libdriverhub_kmi

# Add to ZBI
zbi -o my-image.zbi \
  base-image.zbi \
  --entry bin/driverhub_runner=out/default/host_x64/driverhub_runner \
  --entry lib/libdriverhub_kmi.so=out/default/host_x64/libdriverhub_kmi.so \
  --entry data/modules/my_early_driver.ko=prebuilt/my_early_driver.ko
```

The runner's `.cml` includes an optional `/boot` directory capability for
this scenario — it falls back to `/boot` when `/pkg` is not available.

## 4. Board Driver Integration

### Platform device matching

The DFv2 bus driver component (`driverhub-component`) binds to platform
devices. Your board driver must create a platform device node that matches
DriverHub's bind rules (`meta/driverhub.bind`):

```
using fuchsia.platform;

fuchsia.platform.DRIVER_FRAMEWORK_VERSION == 2;
fuchsia.BIND_PLATFORM_DEV_VID == 0x00;  // PDEV_VID_GENERIC
fuchsia.BIND_PLATFORM_DEV_DID == 0x01;  // PDEV_DID_DRIVERHUB
```

### Creating the platform device in your board driver

Your board driver must create a platform device node that DriverHub can
bind to. Here is an example in C++ using the Fuchsia platform bus API:

```cpp
#include <lib/ddk/platform-defs.h>

// In your board driver's Init() or AddDevices():
static const pbus_dev_t driverhub_dev = {
    .name = "driverhub",
    .vid = PDEV_VID_GENERIC,
    .did = 0x01,  // PDEV_DID_DRIVERHUB
    // Optional: MMIO/IRQ resources for .ko modules that need hardware access.
    .mmio_list = driverhub_mmios,
    .mmio_count = std::size(driverhub_mmios),
    .irq_list = driverhub_irqs,
    .irq_count = std::size(driverhub_irqs),
};

zx_status_t status = pbus_.DeviceAdd(&driverhub_dev);
if (status != ZX_OK) {
    zxlogf(ERROR, "Failed to add driverhub platform device: %s",
           zx_status_get_string(status));
}
```

### MMIO and IRQ resource mapping

If your `.ko` modules need hardware access, the board driver exposes MMIO
and IRQ resources through the platform device. These are then available to
modules via `platform_get_resource()` and `platform_get_irq()` shims:

```cpp
static const pbus_mmio_t driverhub_mmios[] = {
    {
        .base = 0xFF800000,  // Hardware register base
        .length = 0x10000,   // 64KB region
    },
};

static const pbus_irq_t driverhub_irqs[] = {
    {
        .irq = 42,           // GIC SPI interrupt number
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
};
```

The board driver exposes resources (MMIO, IRQ) that `.ko` modules need via
the platform device protocol.

### Device tree configuration

DriverHub reads a device tree blob to determine which modules to load.
Bundle the DTB as a component resource:

```gn
resource("my-board-dtb") {
  sources = [ "my-board.dtb" ]
  outputs = [ "data/board.dtb" ]
}

fuchsia_component("driverhub-component") {
  manifest = "meta/driverhub.cml"
  deps = [
    "//src/drivers/driverhub:driverhub-driver",
    ":my-board-dtb",
  ]
}
```

## 5. Capability Routing

Each `.ko` module component must declare the FIDL protocols it needs, and
these must be routed from the appropriate providers.

### Common capabilities for .ko modules

| Capability | Provider | Purpose |
|-----------|----------|---------|
| `fuchsia.driverhub.SymbolRegistry` | DriverHub runner | Cross-module symbol resolution |
| `fuchsia.driverhub.DeviceFs` | DriverHub runner | Starnix procfs/sysfs/devfs bridge |
| `fuchsia.driverhub.FirmwareLoader` | Starnix / firmware svc | Firmware blob loading for `request_firmware()` |
| `fuchsia.process.Launcher` | Parent realm | Child process creation for module isolation |
| `fuchsia.hardware.i2c.Service` | I2C driver | I2C bus access |
| `fuchsia.hardware.spi.Service` | SPI driver | SPI bus access |
| `fuchsia.hardware.gpio.Service` | GPIO driver | GPIO pin access |
| `fuchsia.hardware.platform.device.Service` | Board driver | MMIO/IRQ resources |
| `fuchsia.kernel.VmexResource` | Root job | Executable VMO creation |

### Example routing in parent realm

```json5
{
    offer: [
        {
            protocol: "fuchsia.driverhub.SymbolRegistry",
            from: "#driverhub-runner",
            to: "#my-ko-module",
        },
        {
            service: "fuchsia.hardware.i2c.Service",
            from: "#i2c-driver",
            to: "#my-ko-module",
        },
    ],
}
```

## 6. Verification

After building, verify the integration:

```shell
# Build the product
fx build

# Check that DriverHub packages are included
fx list-packages | grep driverhub

# Boot and check runner is running
ffx component list | grep driverhub

# Check runner health
ffx component show driverhub-runner

# List loaded .ko modules (via inspect)
ffx inspect show 'core/driverhub-runner'

# Run tests
fx test driverhub-tests
```

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Runner not starting | Missing `VmexResource` or `ProcessLauncher` routing | Add `fuchsia.kernel.VmexResource` and `fuchsia.process.Launcher` to offers |
| Module fails to load | Missing symbol in KMI shim | Check `driverhub` log for "unresolved symbol" |
| Module crashes on init | Unshimmed kernel API called | Add stub to `src/shim/` and rebuild |
| Bind failure | Missing capability routing | Check `.cml` `use` declarations match offers |
| DTB not found | Resource not bundled | Verify DTB in `fuchsia_component` deps |

## 7. Production Considerations

### Signed modules

For production builds, consider enabling module signature verification
before loading. DriverHub can check signatures against the GKI signing
key embedded as a resource.

### Audit logging

Enable structured logging for module load/unload events. DriverHub logs
to `syslog` by default — ensure the log sink is routed in production
component manifests.

### Resource limits

Each `.ko` process has its own memory allocation. Use Fuchsia's job
policies to limit memory and CPU per module process. Set these in the
runner's `.cml` or via the component framework.
