# Fuchsia Integration Notes

This document tracks what needs to change to move DriverHub from the current
host-only development build to a full Fuchsia DFv2 component.

## ELF Loader

- **Swap to elfldltl**: Replace the direct ELF parser in `module_loader.cc`
  with Fuchsia's `elfldltl` library for ELF loading.
- **Memory protection**: Replace the single RWX `mmap` with separate VMOs:
  - `RX` for `.text` (code sections)
  - `RW` for `.data` / `.bss`
  - `R` for `.rodata`
- **Remove MAP_32BIT**: Not needed on AArch64; relocations handle full 64-bit
  address space natively.

## KMI Shim Layer

| Shim | Current (host) | Fuchsia target |
|------|---------------|----------------|
| Memory (`kmalloc`, `vmalloc`) | `malloc` / `calloc` | `zx_vmar_map` / `zx_vmo_create` |
| Logging (`printk`) | `fprintf(stderr)` | `FX_LOG` / structured logging |
| Sync (`mutex`, `spinlock`) | `std::mutex` / `std::atomic_flag` | `sync_mutex_t` / `zx_futex` |
| Time (`jiffies`, `msleep`) | `clock_gettime` / `nanosleep` | `zx_clock_get_monotonic` / `zx_nanosleep` |
| Timers (`add_timer`) | Stub (no-op) | `async::PostDelayedTask` or timer thread |
| Device-managed (`devm_*`) | `malloc` (no auto-free) | Track allocations; free on device removal |

## Platform Device / Driver Model

- **FIDL binding**: `platform_driver_register` should connect to
  `fuchsia.hardware.platform.device/Device` FIDL protocol.
- **Bind rules**: Generate DFv2 bind rules from DT `compatible` strings.
- **Resource mapping**: `platform_get_resource(IORESOURCE_MEM)` should map to
  MMIO via `fuchsia.hardware.platform.device/GetMmio`.
- **IRQ delivery**: `platform_get_irq` should map to
  `fuchsia.hardware.platform.device/GetInterrupt`.

## Bus Driver (DFv2 Component)

- Create a `fuchsia_driver` component manifest (`.cml`).
- Implement the `fuchsia.driver.framework/Driver` protocol.
- The bus driver should:
  1. Read device tree configuration (from component resources or DT visitor).
  2. Create child nodes for each `.ko` module.
  3. Expose per-module FIDL capabilities to downstream Fuchsia drivers.

## Subsystem Shims -> FIDL

| Subsystem | FIDL protocol target |
|-----------|---------------------|
| USB | `fuchsia.hardware.usb/Usb` |
| I2C | `fuchsia.hardware.i2c/Device` |
| SPI | `fuchsia.hardware.spi/Device` |
| GPIO | `fuchsia.hardware.gpio/Gpio` |
| RTC | `fuchsia.hardware.rtc/Device` |

Each subsystem shim translates Linux API calls into FIDL requests, marshalling
data structures as needed.

## Device Tree

- **StaticDtProvider**: Parse `.dtb` blobs bundled as component resources.
- **VisitorDtProvider**: Receive device tree nodes from the Fuchsia DT visitor
  framework for dynamic enumeration.
- Both need implementation (currently header-only in `src/dt/dt_provider.h`).

## Build System

- Add `BUILD.gn` rules for the Fuchsia build.
- Package `.ko` files as Fuchsia package resources.
- Package `.dtb` files as component resources.
- Wire up `fx test driverhub-tests` to run the test suite.

## AArch64 Cross-Compilation

- Primary GKI target is ARM64. The host build currently runs on x86_64.
- AArch64 relocation types are already implemented in the loader.
- To test on host: install `aarch64-linux-gnu-gcc` and `qemu-aarch64-static`.
- On Fuchsia: builds natively for ARM64, no cross-compilation needed.

## Testing Strategy

- Host tests (current): validate loader, shims, and symbol resolution on x86_64.
- Fuchsia component tests: validate DFv2 lifecycle and FIDL integration.
- Hardware-in-the-loop: load real GKI `.ko` modules on ARM64 Fuchsia devices.
