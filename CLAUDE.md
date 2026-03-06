# CLAUDE.md — AI Assistant Guide for DriverHub

## Project Overview

DriverHub is a compatibility layer that enables Fuchsia's Driver Framework v2
(DFv2) to load and use Android Linux GKI (Generic Kernel Image) kernel modules.
It uses `elfldltl` to load `.ko` files, resolves their symbols against a C++
shim library implementing the Linux KMI, and exposes each module as an
independent DFv2 child node.

See `docs/design.md` for the full architectural design.

## Repository Structure

```
driverhub/
├── CLAUDE.md              # AI assistant guidelines (this file)
├── README.md              # Project overview
├── BUILD.gn               # Fuchsia build rules
├── docs/
│   └── design.md          # Architecture design document
├── src/
│   ├── bus_driver/         # Top-level DFv2 bus driver component
│   ├── module_node/        # Per-module DFv2 child node
│   ├── loader/             # elfldltl-based .ko loader + relocation
│   ├── shim/
│   │   ├── kernel/         # Core KMI shims (kmalloc, printk, sync, etc.)
│   │   └── subsystem/      # Subsystem shims (USB, I2C, SPI, GPIO) → FIDL
│   ├── symbols/            # EXPORT_SYMBOL registry + intermodule resolution
│   └── dt/                 # Device tree parsing (static blob + DT visitor)
└── tests/                  # Unit and integration tests
```

## Key Concepts

- **Bus driver**: Single DFv2 node that owns module loading and child creation.
- **Module node**: Each `.ko` gets its own DFv2 child node for independent
  lifecycle and clean binding with existing Fuchsia drivers.
- **KMI shim library**: C++ implementations of Linux kernel APIs that delegate
  to Fuchsia primitives and FIDL protocols.
- **Symbol registry**: Resolves KMI symbols and intermodule `EXPORT_SYMBOL`
  dependencies.
- **DT config**: Device tree (static blob or Fuchsia DT visitor) drives which
  modules are loaded and how they are configured.

## Tech Stack

- **Language**: C++ (shim layer and DFv2 components)
- **ELF loading**: `elfldltl` (Fuchsia's ELF loading library)
- **IPC**: FIDL (Fuchsia Interface Definition Language)
- **Build system**: GN + Fuchsia build (BUILD.gn)
- **Target arch**: ARM64 (primary Android GKI target)

## Build & Test Commands

```shell
# Build (within Fuchsia tree)
fx set <product>.<board> --with //src/drivers/driverhub
fx build

# Run tests
fx test driverhub-tests

# Lint
# Follow Fuchsia C++ style (enforced by clang-format / clang-tidy)
```

## Development Workflow

### Branch Conventions

- Feature branches: `feature/<description>`
- Bug fix branches: `fix/<description>`
- Documentation branches: `docs/<description>`
- Always branch from `main`

### Commit Messages

Follow conventional commit format:

```
<type>(<scope>): <short summary>

<optional body>
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `ci`

Scopes: `loader`, `shim`, `bus`, `node`, `symbols`, `dt`, `usb`, `i2c`, etc.

Examples:
- `feat(loader): implement ET_REL relocation for ARM64`
- `feat(shim/usb): add usb_register_driver shim backed by FIDL`
- `fix(symbols): handle duplicate EXPORT_SYMBOL registration`
- `docs: update design.md with threading model`

### Pull Requests

- Keep PRs focused on a single concern
- Include a clear description of what changed and why
- Reference related issues when applicable

## Code Style & Conventions

### General Principles

- Follow [Fuchsia C++ style](https://fuchsia.dev/fuchsia-src/development/languages/c-cpp/cpp-style)
- Write clear, readable code over clever code
- Keep functions focused and small
- Use descriptive names for variables, functions, and types
- Add comments only where the intent isn't obvious from the code itself

### Shim Implementation Conventions

- Each Linux API shim should be in a header+source pair named after the Linux
  header it replaces (e.g., `shim/kernel/slab.h` for `kmalloc`).
- Shim functions should match the Linux function signature exactly so `.ko`
  symbol resolution works without modification.
- Use `extern "C"` linkage for all shim function exports.
- Map Linux error codes (`-ENOMEM`, etc.) internally; convert to `zx_status_t`
  at FIDL boundaries.
- Document which KMI symbols each shim file provides.

### File Organization

- Group related files together in directories
- Keep a flat structure where possible; avoid deep nesting
- Name files descriptively

## Key Architectural Decisions

1. **Userspace execution**: GKI modules run in userspace with shimmed kernel
   APIs, not in a VM or kernel context.
2. **Per-module DFv2 nodes**: Each `.ko` is an independent DFv2 child for
   isolation and native Fuchsia integration.
3. **Subsystem-level shims**: USB, I2C, SPI, GPIO shims enable whole classes
   of modules without per-driver glue.
4. **Device tree driven**: Module loading is configured via device tree, bridged
   to DFv2 bind rules via `compatible` strings.
5. **elfldltl for loading**: Leverages Fuchsia's existing ELF library rather
   than a custom loader.

## Environment & Configuration

- Do not commit secrets, credentials, or `.env` files
- Device tree blobs (`.dtb`) used for static configuration are bundled as
  component resources
- Module `.ko` files are packaged as Fuchsia package resources

## Guidelines for AI Assistants

1. **Read before writing** — Always read existing code before modifying it.
2. **Read `docs/design.md`** — Understand the architecture before making
   changes to core components.
3. **Minimal changes** — Only change what's needed to accomplish the task.
4. **No speculative features** — Don't add functionality that wasn't requested.
5. **Shim fidelity** — Shim functions must match Linux signatures exactly.
   Use `extern "C"` linkage.
6. **Run tests** — After making changes, run the test suite.
7. **Check for lint** — Follow Fuchsia C++ style.
8. **Commit atomically** — Each commit should represent one logical change.
9. **Don't commit secrets** — Never commit `.env`, credentials, API keys,
   or tokens.
10. **Don't commit `.ko` binaries** — Reference them as test fixtures or
    package resources, not checked-in blobs.
