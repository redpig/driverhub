# DriverHub + Starnix Integration: procfs/sysfs Bridge

## Problem Statement

Linux .ko kernel modules create entries in `/proc`, `/sys`, and `/dev` as part
of their normal initialization (e.g., `sysfs_create_group()`,
`proc_create()`, `device_add()`, `class_create()`). Linux userspace binaries
— firmware loaders, HALs, configuration tools — depend on these filesystem
entries to discover, configure, and interact with hardware.

DriverHub's KMI shim layer already intercepts all these calls and records
the entries in in-process registries (see `src/shim/subsystem/fs.cc`). But
currently this data is trapped inside each module process and not visible to
the rest of the system.

Starnix implements procfs and sysfs as synthetic filesystems within its Linux
compatibility layer. It populates them from its own internal state. There is
currently no mechanism for external Fuchsia components to inject entries into
Starnix's VFS.

**Goal**: Bridge DriverHub's .ko module state into Starnix's procfs/sysfs so
that Linux binaries running under Starnix can interact with DriverHub-loaded
hardware drivers through the standard Linux filesystem interface.

## Architecture

```
┌─────────────────────────────┐    ┌──────────────────────────────┐
│     Starnix Container       │    │    DriverHub Runner          │
│                             │    │                              │
│  ┌───────────────────────┐  │    │  ┌────────────────────────┐  │
│  │ Linux Binary (HAL)    │  │    │  │ .ko Module Process     │  │
│  │  reads /sys/class/... │  │    │  │  calls sysfs_create()  │  │
│  │  writes /proc/...     │  │    │  │  calls device_add()    │  │
│  └──────────┬────────────┘  │    │  └──────────┬─────────────┘  │
│             │               │    │             │                │
│  ┌──────────▼────────────┐  │    │  ┌──────────▼─────────────┐  │
│  │ Starnix VFS           │  │    │  │ libdriverhub_kmi.so    │  │
│  │  sysfs_fs.rs          │◄─┼─FIDL──┤  fs.cc shim            │  │
│  │  procfs_fs.rs         │  │    │  │  (registries)          │  │
│  │  devfs proxy          │  │    │  └────────────────────────┘  │
│  └───────────────────────┘  │    │                              │
└─────────────────────────────┘    └──────────────────────────────┘
         ▲                                      ▲
         │        FIDL protocols                │
         └──────────────────────────────────────┘
           fuchsia.driverhub.DeviceFs
           fuchsia.driverhub.FirmwareLoader
```

## FIDL Protocol Design

### fuchsia.driverhub.DeviceFs

This protocol allows Starnix to discover and interact with sysfs/procfs/devfs
entries created by DriverHub's .ko modules.

```fidl
library fuchsia.driverhub;

/// Represents a single filesystem entry (sysfs attribute, proc entry, etc.)
type FsEntry = table {
    /// Full path as it would appear in Linux (e.g., "/sys/class/rtc/rtc0")
    1: path string:MAX;

    /// Entry type
    2: kind FsEntryKind;

    /// For regular files: the initial content (if readable)
    3: content vector<byte>:MAX;

    /// Permissions (Linux mode bits)
    4: mode uint32;

    /// Device major/minor (for char/block device nodes)
    5: major uint32;
    6: minor uint32;
};

type FsEntryKind = flexible enum : uint8 {
    DIRECTORY = 1;
    REGULAR = 2;
    SYMLINK = 3;
    CHAR_DEVICE = 4;
    BLOCK_DEVICE = 5;
};

/// Event fired when a .ko module creates or removes fs entries.
type FsEvent = table {
    1: kind FsEventKind;
    2: entry FsEntry;
};

type FsEventKind = flexible enum : uint8 {
    ADDED = 1;
    REMOVED = 2;
    MODIFIED = 3;
};

/// Served by DriverHub, consumed by Starnix.
///
/// Starnix queries this to populate /sys, /proc, and /dev with entries
/// created by Linux kernel modules loaded via DriverHub.
@discoverable
protocol DeviceFs {
    /// Get all current filesystem entries (for initial sync at mount time).
    flexible ListEntries() -> (resource struct {
        entries vector<FsEntry>:MAX;
    }) error zx.Status;

    /// Watch for filesystem entry changes (new devices, removed entries, etc.)
    flexible Watch(resource struct {
        watcher server_end:DeviceFsWatcher;
    });

    /// Read a sysfs/procfs attribute value.
    /// Dispatches to the .ko module's show() callback.
    flexible ReadAttribute(struct {
        path string:MAX;
    }) -> (struct {
        content vector<byte>:MAX;
    }) error zx.Status;

    /// Write a sysfs/procfs attribute value.
    /// Dispatches to the .ko module's store() callback.
    flexible WriteAttribute(struct {
        path string:MAX;
        content vector<byte>:MAX;
    }) -> () error zx.Status;

    /// Open a character device node for I/O.
    /// Returns a channel implementing fuchsia.io/File for read/write/ioctl.
    flexible OpenDevice(resource struct {
        path string:MAX;
        server server_end:fuchsia.io.File;
    }) -> () error zx.Status;
};

/// Event stream for filesystem changes.
protocol DeviceFsWatcher {
    /// Fired when entries are added, removed, or modified.
    flexible -> OnEvent(FsEvent);
};
```

### fuchsia.driverhub.FirmwareLoader

For `request_firmware()` calls, we bridge to Starnix-hosted firmware loaders.

```fidl
/// Served by Starnix (or a firmware service), consumed by DriverHub.
///
/// When a .ko module calls request_firmware(), the KMI shim sends this
/// request. A Starnix-hosted firmware loader (or Fuchsia firmware service)
/// finds the firmware blob and returns it.
@discoverable
protocol FirmwareLoader {
    /// Request a firmware blob by name.
    /// The name matches what the Linux driver passes to request_firmware()
    /// (e.g., "iwlwifi-cc-a0-77.ucode").
    flexible RequestFirmware(struct {
        name string:MAX;
    }) -> (resource struct {
        firmware zx.Handle:VMO;
        size uint64;
    }) error zx.Status;
};
```

## Integration Points

### 1. Sysfs Attributes (Read/Write)

When a .ko module calls `sysfs_create_group(kobj, grp)`, the shim:
1. Records the kobject path and attribute group in a local registry
2. Publishes an `FsEvent::ADDED` for each attribute via `DeviceFsWatcher`
3. Starnix picks up the event and creates a synthetic file in its sysfs

When Starnix receives a read on `/sys/devices/.../some_attr`, it calls
`DeviceFs::ReadAttribute(path)`. The runner routes this to the module's
`attribute->show()` callback. Writes use `store()`.

### 2. procfs Entries

Same pattern as sysfs. When a .ko calls `proc_create()`, the shim publishes
an event. Starnix creates a synthetic `/proc/...` entry. Reads dispatch to
the module's `proc_fops->read` or `seq_file->show`.

### 3. Device Nodes (/dev)

When a .ko registers a char device (`register_chrdev`, `misc_register`), the
shim:
1. Records the device with major/minor numbers
2. Publishes an `FsEvent::ADDED` with `FsEntryKind::CHAR_DEVICE`
3. Starnix creates `/dev/foo` in its devfs

When a Linux binary opens `/dev/foo`, Starnix calls `DeviceFs::OpenDevice()`
which returns a channel that proxies `read`/`write`/`ioctl` to the .ko
module's `file_operations`.

### 4. Firmware Loading

When a .ko calls `request_firmware("foo.bin", ...)`:
1. The shim calls `FirmwareLoader::RequestFirmware("foo.bin")`
2. Starnix-hosted `udev` or a Fuchsia firmware service finds the blob
3. The firmware VMO is mapped into the module's address space
4. `request_firmware()` returns success to the .ko

**Alternative**: DriverHub can also look for firmware in its own package
(`/pkg/data/firmware/`) first, falling back to the FirmwareLoader service
only if not found locally.

### 5. uevent / Hotplug

When a .ko calls `device_add()` or `kobject_uevent()`, the shim:
1. Publishes an `FsEvent` via `DeviceFsWatcher`
2. Starnix translates this to a netlink uevent for `udev` in its container

This allows Starnix-hosted `udev` to react to new hardware just like on
real Linux.

## Lifecycle & Boot Order

```
Boot sequence:
1. DriverHub runner starts (Fuchsia component)
2. DriverHub loads .ko modules from device tree config
3. .ko modules call device_add(), sysfs_create(), etc.
4. DriverHub's DeviceFs service is now populated
5. Starnix container starts
6. Starnix mounts /sys, /proc, /dev
7. Starnix calls DeviceFs::ListEntries() to populate initial state
8. Starnix calls DeviceFs::Watch() for ongoing updates
9. Linux binaries in Starnix see populated /sys, /proc, /dev
```

DriverHub modules **can and should load before Starnix**. The `DeviceFs`
protocol supports this by providing `ListEntries()` for initial state
sync and `Watch()` for live updates. Entries created before Starnix starts
are buffered and returned on the first `ListEntries()` call.

## Implementation Plan

### Phase 1: DeviceFs Protocol (DriverHub side)

1. Define `fuchsia.driverhub.DeviceFs` FIDL protocol
2. Add `DeviceFsServer` to the runner that aggregates state from all module
   processes' shim registries
3. Modify `libdriverhub_kmi.so` shim functions (`sysfs_create_group`,
   `proc_create`, `device_add`, etc.) to forward events to the runner via
   a channel
4. Runner's `DeviceFsServer` maintains a combined view and serves it to
   Starnix

### Phase 2: Starnix Integration

1. Starnix adds a `driverhub` filesystem type that connects to
   `fuchsia.driverhub.DeviceFs`
2. Mount this at `/sys/devices/driverhub/` (or merge into the existing
   sysfs tree)
3. Attribute reads/writes proxy through `DeviceFs::ReadAttribute/WriteAttribute`
4. Character device opens proxy through `DeviceFs::OpenDevice`

### Phase 3: Firmware Bridge

1. Define `fuchsia.driverhub.FirmwareLoader` FIDL protocol
2. Implement in the DriverHub shim's `request_firmware()` path
3. Starnix (or a standalone Fuchsia component) serves the firmware blobs
   from `/data/firmware/` or a Fuchsia package

### Phase 4: uevent / Hotplug

1. Extend `DeviceFsWatcher` to carry uevent-compatible data
2. Starnix translates watcher events to netlink uevents for its containers
3. `udev` in Starnix discovers DriverHub devices automatically

## Starnix Internals (as of 2026-03)

Understanding Starnix's current architecture is critical for integration.

### Starnix sysfs Implementation

Starnix's sysfs is implemented in Rust at `src/starnix/kernel/core/fs/sysfs/`:
- `fs.rs` — filesystem mounting, lazy init, top-level directories
- `device_directory.rs` — per-device sysfs entries (dev, uevent, queue)
- `kernel_directory.rs` — `/sys/kernel` entries
- `power_directory.rs` — `/sys/power` entries
- `cpu_class_directory.rs` — `/sys/devices/system/cpu` entries

The sysfs init creates directories for `/sys/fs`, `/sys/block`,
`/sys/bus` (mmc, platform, pci), `/sys/class` (backlight, bdi, mmc_host,
net), `/sys/dev`, `/sys/firmware`, `/sys/kernel`, `/sys/power`,
`/sys/leds`, `/sys/module`, and `/sys/devices`. **Many entries are stubs**
(marked with `bug_ref` macros), meaning the surface area is limited and
actively being expanded.

### Device Subsystem

Starnix has a device subsystem at `src/starnix/kernel/core/device/`:
- `registry.rs` — central `DeviceRegistry` for char/block devices
- `kobject.rs` — Linux-like kobject abstraction
- `kobject_store.rs` — hierarchical KObject storage backing sysfs

The `DeviceRegistry` manages device registration, major/minor assignment,
and sysfs population. The `KObjectStore` organizes devices into buses
(mmc, platform, pci) and classes (block, thermal, graphics, input,
memory, network, etc.).

### No External Injection Mechanism

**There is no public FIDL protocol for external Fuchsia components to
inject entries into Starnix's sysfs or procfs.** The `DeviceRegistry` and
`KObjectStore` are internal Rust APIs within the Starnix process. This
means the `fuchsia.driverhub.DeviceFs` protocol proposed here would be
a new capability class for Fuchsia — the first external sysfs provider.

### How Starnix Currently Gets Hardware Info

Starnix obtains hardware information through:
1. **FIDL capability routing**: `fuchsia.hardware.*` protocols from DFv2
   drivers are routed into the Starnix container's namespace
2. **Internal shim code**: Starnix kernel code consumes FIDL protocols
   and surfaces them as Linux-compatible device nodes
3. **Remote Binder**: For Android HALs, translates Binder IPC to FIDL
   calls, enabling Android HAL services to run as native Fuchsia
   components while remaining callable from Android processes in Starnix

### Integration Strategy

Given that Starnix has no external sysfs injection, we have two options:

**Option A (Recommended): New FIDL protocol**
- DriverHub exposes `fuchsia.driverhub.DeviceFs`
- Starnix adds a new filesystem plugin that consumes this protocol
- Devices appear under `/sys/devices/platform/driverhub/`
- Cleanest separation; no modifications to Starnix's core sysfs

**Option B: Starnix kernel module**
- Write a Rust module inside Starnix that consumes a FIDL protocol
  and registers devices through `DeviceRegistry` directly
- Devices appear in the native sysfs hierarchy (e.g., `/sys/class/rtc/`)
- More natural to Linux binaries, but tighter coupling to Starnix internals
- Would need `Weak<dyn BlockDeviceInfo>` implementations for block devices

**Recommended approach**: Start with Option A (cleaner, less Starnix
modification), then migrate to Option B for specific device classes where
Linux userspace expects exact sysfs paths (e.g., WiFi tools expect
`/sys/class/net/wlan0/`).

## Design Considerations

### Why FIDL and not shared memory?

Sysfs/procfs operations are typically low-frequency (device discovery,
configuration reads). FIDL provides type safety, versioning, and clean
error handling. The performance overhead is negligible compared to the
actual I/O operations these attributes represent.

### Why not have .ko modules run inside Starnix?

Starnix implements the Linux *syscall* interface. Kernel modules don't use
syscalls — they use internal kernel APIs (`kmalloc`, spinlocks, interrupt
handlers, DMA). DriverHub's KMI shim layer is specifically designed for
these kernel-internal APIs. The two systems are complementary:
- **DriverHub**: runs kernel-space code (.ko) with kernel API shims
- **Starnix**: runs user-space code (Linux binaries) with syscall shims

### Why not use Remote Binder?

Remote Binder bridges Android AIDL HAL interfaces, not Linux kernel
sysfs/procfs. Linux userspace tools (firmware loaders, `lspci`, `lsusb`,
`iw`) interact through filesystem APIs, not Binder. The DeviceFs approach
preserves this standard Linux interface.

### What about existing Fuchsia drivers?

DriverHub-loaded .ko modules expose hardware to Fuchsia via DFv2 bind rules
(native Fuchsia integration). The Starnix bridge is *in addition to* this —
it allows Linux userspace tools to work alongside native Fuchsia drivers.
Both views of the same hardware can coexist.

### Namespace collision with Starnix's built-in sysfs

Starnix already has synthetic sysfs entries for CPU info, memory, etc.
DriverHub devices should be mounted under a subtree (e.g.,
`/sys/devices/platform/driverhub/`) to avoid collisions. For specific
device classes that Linux tools expect at exact paths, Option B
(Starnix kernel module) integrates more naturally.
