# DriverHub Design Document

## Overview

DriverHub is a compatibility layer that enables Fuchsia's Driver Framework v2
(DFv2) to load and use Android Linux GKI (Generic Kernel Image) kernel modules.
It achieves this by acting as a DFv2 bus driver that loads `.ko` ELF relocatable
objects via `elfldltl`, resolves their symbols against a shim library
implementing the Linux KMI (Kernel Module Interface), and exposes each loaded
module as an independent DFv2 child node.

## Goals

- Load unmodified Android GKI `.ko` modules in Fuchsia userspace.
- Provide KMI shim implementations backed by FIDL calls to Fuchsia services.
- Support subsystem-level registration (USB, I2C, SPI, GPIO) so that enabling
  new modules requires minimal per-driver work.
- Enumerate each loaded module as its own DFv2 node for clean integration with
  the existing Fuchsia driver ecosystem.
- Support device tree-driven configuration, both static (DT blob) and dynamic
  (Fuchsia DT visitor).

## Architecture

```
┌─────────────────────────────────────────────────┐
│  DriverHub Bus Driver (DFv2 component)          │
│                                                 │
│  ┌─────────────┐    ┌───────────────────────┐   │
│  │ DT Config    │───▶│ Module Loader          │   │
│  │ (static or   │    │ (elfldltl)             │   │
│  │  visitor)    │    └───────────┬────────────┘   │
│  └─────────────┘                │               │
│                       ┌─────────▼────────────┐  │
│                       │ Symbol Registry       │  │
│                       │ (KMI + intermodule)   │  │
│                       └─────────┬────────────┘  │
│                                 │               │
│            ┌────────────────────┼───────────┐   │
│            │                    │           │   │
│  ┌─────────▼───────┐ ┌─────────▼───────┐ ┌─▼─┐ │
│  │ Module Node      │ │ Module Node      │ │...│ │
│  │ (DFv2 child)     │ │ (DFv2 child)     │ │   │ │
│  │ e.g. usb_wifi.ko │ │ e.g. i2c_touch.ko│ │   │ │
│  └────────┬─────────┘ └────────┬─────────┘ └───┘ │
│           │ FIDL               │ FIDL             │
└───────────┼────────────────────┼─────────────────┘
            │                    │
     ┌──────▼───────┐    ┌──────▼───────┐
     │ Fuchsia USB   │    │ Fuchsia I2C   │
     │ Stack         │    │ Stack         │
     └──────────────┘    └──────────────┘
```

## Components

### Bus Driver (`src/bus_driver/`)

The top-level DFv2 component. Responsible for:

- Initializing the shim library and symbol registry.
- Parsing device tree configuration to discover which modules to load.
- Orchestrating module loading and child node creation.
- Managing the shared lifecycle of the compatibility layer.

Appears in the DFv2 topology as a single node. Child module nodes hang off it.

### Module Node (`src/module_node/`)

A DFv2 child node representing a single loaded `.ko` module. Each module node:

- Owns the loaded module's memory and state.
- Exposes DFv2 bind properties derived from device tree `compatible` strings.
- Manages module lifecycle: `module_init()` on bind, `module_exit()` on unbind.
- Can be independently started, stopped, and restarted without affecting siblings.

This design enables integration with existing Fuchsia drivers — a GKI USB
driver appears as a normal USB node to the rest of the system.

### Module Loader (`src/loader/`)

Uses `elfldltl` to load `.ko` files (ELF `ET_REL` relocatable objects):

1. Parse the ELF headers and section table.
2. Allocate memory for loadable sections (`.text`, `.data`, `.bss`, `.rodata`).
3. Apply relocations, resolving undefined symbols against:
   - The KMI shim library (kernel API implementations).
   - The symbol registry (intermodule exports).
4. Locate and return pointers to `init_module` / `cleanup_module` entry points.

#### Key considerations

- `.ko` files are `ET_REL`, not `ET_DYN` — they require full relocation, not
  just PLT/GOT fixups.
- Architecture-specific relocation types (ARM64 for most Android GKI targets)
  must be handled.
- `.modinfo` section parsing extracts module metadata (author, description,
  license, alias, `vermagic`).
- `vermagic` validation may be relaxed or skipped since we are not running in a
  real Linux kernel.

### KMI Shim Library (`src/shim/`)

A shared library exporting symbols that match the Linux KMI surface. When the
module loader resolves symbols, they point to these shim implementations.

#### Core kernel shims (`src/shim/kernel/`)

| Linux API | Shim strategy |
|-----------|--------------|
| `kmalloc`, `kfree`, `kzalloc` | Map to userspace allocator (`malloc`/`free` or custom pool) |
| `vmalloc`, `vfree` | Map to `zx_vmar_map` / `zx_vmar_unmap` |
| `printk`, `dev_err/warn/info` | Map to Fuchsia `FX_LOG` |
| `mutex_lock/unlock` | Map to `std::mutex` or `sync_mutex_t` |
| `spin_lock/unlock` | Map to `std::atomic` or Fuchsia spinlock |
| `wait_event`, `wake_up` | Map to `zx_futex` or condition variables |
| `schedule_work`, `queue_work` | Map to `async::Loop` / `async::PostTask` |
| `request_irq`, `free_irq` | Map to `zx_interrupt_wait` via IRQ FIDL protocol |
| `ioremap`, `iounmap` | Map to `zx_vmo_create_physical` + `zx_vmar_map` via platform device FIDL |
| `dma_alloc_coherent` | Map to `zx_bti_pin` / `zx_vmo_create_contiguous` via BTI FIDL |
| `copy_from_user`, `copy_to_user` | No-op or memcpy (userspace context) |
| `jiffies`, `msleep`, `udelay` | Map to `zx_clock_get_monotonic` / `zx_nanosleep` |

#### Subsystem shims (`src/shim/subsystem/`)

These are the high-value shims that make whole classes of modules work without
per-driver effort.

**USB (`usb.h`)**
- `usb_register_driver` → Registers with Fuchsia USB stack via
  `fuchsia.hardware.usb/Usb` FIDL protocol.
- `usb_submit_urb` → Translates to USB request FIDL calls.
- `usb_control_msg` → Maps to control transfer FIDL calls.
- Endpoint management, interface claiming mapped to Fuchsia USB primitives.

**I2C (`i2c.h`)**
- `i2c_add_driver` → Binds to `fuchsia.hardware.i2c/Device` FIDL.
- `i2c_transfer` → Maps to I2C read/write FIDL transactions.

**SPI (`spi.h`)**
- `spi_register_driver` → Binds to `fuchsia.hardware.spi/Device` FIDL.
- `spi_sync`, `spi_async` → Maps to SPI FIDL transfer calls.

**GPIO (`gpio.h`)**
- `gpio_request`, `gpio_direction_input/output`, `gpio_get/set_value` →
  Maps to `fuchsia.hardware.gpio/Gpio` FIDL.

**Platform / Device Model (`platform.h`)**
- `platform_driver_register` → Registers as a platform device via
  `fuchsia.hardware.platform.device/Device` FIDL.
- `platform_get_resource` → Retrieves MMIO/IRQ resources from DFv2 node
  properties (originally sourced from device tree).
- `of_match_table` → Used to derive DFv2 bind rules from `compatible` strings.

### Symbol Registry (`src/symbols/`)

Manages symbol resolution across module boundaries:

- **KMI symbols**: Populated at startup from the shim library's export table.
- **Intermodule symbols**: When a module calls `EXPORT_SYMBOL(foo)`, the
  registry records `foo`'s address. Subsequent module loads can resolve against
  it.
- **Load ordering**: The bus driver must respect module dependencies (extracted
  from `.modinfo` `depends=` fields) and load in topological order.

```cpp
class SymbolRegistry {
 public:
  // Register a symbol (called by EXPORT_SYMBOL shim and at startup for KMI)
  void Register(std::string_view name, void* address);

  // Resolve a symbol by name, returns nullptr if not found
  void* Resolve(std::string_view name) const;

  // Bulk-register all KMI shim symbols
  void RegisterKmiSymbols();

 private:
  std::unordered_map<std::string_view, void*> symbols_;
};
```

### Device Tree Integration (`src/dt/`)

Configures which modules to load and provides device properties to each module's
probe function.

#### Static mode

A pre-compiled device tree blob (`.dtb`) is bundled as a resource with the bus
driver component. Parsed at startup to enumerate modules.

#### Dynamic mode (DT visitor)

Uses Fuchsia's device tree visitor infrastructure to receive device tree nodes
from the board driver. Each visited node with a recognized `compatible` string
triggers module loading.

#### DT-to-DFv2 mapping

| Device tree concept | DFv2 equivalent |
|-------------------|----------------|
| `compatible` string | Bind rule (`fuchsia.BIND_PLATFORM_DEV_DID`) |
| `reg` property | MMIO resource from platform device |
| `interrupts` property | IRQ resource from platform device |
| `status = "okay"` | Node is created and module is loaded |
| Child nodes | Additional DFv2 child nodes |

## Module Lifecycle

```
Bus Driver starts
    │
    ▼
Parse device tree
    │
    ▼
For each DT node with matching .ko:
    │
    ├─ Resolve dependencies (topological sort)
    │
    ├─ Load .ko via elfldltl
    │   ├─ Allocate sections
    │   ├─ Relocate symbols against shim + registry
    │   └─ Parse .modinfo
    │
    ├─ Create DFv2 child node
    │   └─ Set bind properties from DT compatible
    │
    └─ Call module_init()
        └─ Module registers with subsystem (e.g. usb_register)
            └─ Shim creates FIDL connection to Fuchsia service
                └─ Module's probe() called when device matches

          ─ ─ ─ (runtime) ─ ─ ─

On unbind / shutdown:
    │
    ├─ Call module_exit()
    │   └─ Module unregisters from subsystem
    │
    ├─ Tear down FIDL connections
    │
    └─ Free loaded module memory
```

## Threading Model

Linux kernel modules assume multiple execution contexts (process, softirq,
hardirq). In the DriverHub userspace model, these are flattened:

| Linux context | DriverHub mapping |
|--------------|------------------|
| Process context | Default async dispatcher thread |
| Workqueues | `async::Loop` worker threads |
| Softirq / tasklet | Posted tasks on async dispatcher |
| Hardirq (ISR) | Dedicated interrupt-waiting thread per IRQ |
| Timers | `async::TaskClosure` with deadline |

Synchronization primitives (mutexes, spinlocks) remain meaningful since multiple
threads are in play. Spinlocks map to lightweight userspace spinlocks rather
than disabling preemption.

## Error Handling and Isolation

- Each module node runs in its own context. A fault in one module does not
  crash siblings or the bus driver.
- Linux error codes (`-ENOMEM`, `-EINVAL`, etc.) are preserved within the shim
  layer. At the FIDL boundary, they map to `zx_status_t`.
- `BUG()`, `WARN()` and `panic()` are intercepted — `WARN()` logs and
  continues, `BUG()`/`panic()` tear down only the affected module node.

## Future Considerations

- **virtio**: Shim virtio transport to enable virtio-based GKI modules.
- **DebugFS / sysfs shims**: Expose module debug info through Fuchsia inspect.
- **Power management**: Map Linux PM callbacks (`suspend`/`resume`) to Fuchsia
  power framework.
- **Module signing**: Validate GKI module signatures before loading.
- **KMI version checking**: Enforce compatibility with specific GKI KMI
  versions.
