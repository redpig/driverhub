# Rust Migration Plan for DriverHub

## Motivation

DriverHub's loader processes untrusted ELF input from `.ko` files. Memory
safety bugs in ELF parsing, relocation, and symbol resolution could lead
to code execution. Rust's ownership model eliminates classes of bugs
(buffer overflows, use-after-free, double-free) that are most dangerous
in this context.

Secondary benefits:
- `object` crate provides robust, well-tested ELF parsing
- Stronger type system catches logic errors at compile time
- Pattern matching makes relocation dispatch clearer
- Rust tests are built-in (no gtest dependency)

## Scope

### Phase 1: Pure-logic modules (this PR)

Port self-contained, C-ABI-free components that don't interact with `.ko`
code at runtime:

| Component | C++ files | Rust module | Rationale |
|-----------|-----------|-------------|-----------|
| Modinfo parser | `modinfo_parser.{h,cc}` | `modinfo` | Parses untrusted ELF data |
| Dependency sort | `dependency_sort.{h,cc}` | `dependency_sort` | Pure algorithm |
| DT provider | `dt_provider.{h,cc}` | `dt` | Parses untrusted FDT blobs |
| ELF loader | `module_loader.{h,cc}` | `loader` | Parses untrusted ELF, applies relocations |
| Memory allocator | `memory_allocator.{h,cc}`, `mmap_allocator.{h,cc}` | `allocator` | mmap wrapper |
| Symbol registry | `symbol_registry.{h,cc}` | `symbols` | HashMap wrapper (register/resolve) |

### Phase 2: Orchestration (future)

Port the bus driver and module node logic. These are simpler and benefit
from Rust's DFv2 bindings on Fuchsia.

### Out of scope (stays C++)

The **KMI shim library** (`src/shim/`) must remain C++ because:
- Functions must have exact Linux C signatures (`extern "C"`)
- `-mstackrealign` flag needed for GKI ABI compat has no Rust equivalent
- Struct layouts must match Linux kernel headers exactly
- The shim headers are consumed by C code

## Architecture

```
┌──────────────────────────────────────────────────────┐
│  C++ layer (unchanged)                                │
│                                                       │
│  src/main.cc                                          │
│  src/bus_driver/bus_driver.cc  ──┐                    │
│  src/module_node/module_node.cc  │  calls Rust via    │
│  src/shim/**  (stays C++)        │  C FFI bridge      │
│                                  │                    │
│  ┌───────────────────────────────▼──────────────────┐ │
│  │  C FFI bridge headers                             │ │
│  │  rust/driverhub-core/include/driverhub_core.h     │ │
│  └───────────────────────────────┬──────────────────┘ │
│                                  │                    │
│  ┌───────────────────────────────▼──────────────────┐ │
│  │  Rust static library: libdriverhub_core.a         │ │
│  │                                                   │ │
│  │  rust/driverhub-core/src/                         │ │
│  │    lib.rs          — crate root, FFI exports      │ │
│  │    modinfo.rs      — .modinfo parser              │ │
│  │    dependency_sort.rs — topological sort           │ │
│  │    dt.rs           — FDT blob parser              │ │
│  │    loader.rs       — ELF ET_REL loader            │ │
│  │    relocations.rs  — arch-specific relocations    │ │
│  │    allocator.rs    — memory allocator trait+mmap  │ │
│  │    symbols.rs      — symbol registry              │ │
│  └───────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────┘
```

## FFI Strategy

The Rust crate compiles to a **static library** (`libdriverhub_core.a`).
C++ code links against it and calls through a C FFI header.

### Key FFI types

```c
// rust/driverhub-core/include/driverhub_core.h

// Opaque handles
typedef struct DhSymbolRegistry DhSymbolRegistry;
typedef struct DhModuleLoader DhModuleLoader;
typedef struct DhLoadedModule DhLoadedModule;

// Module metadata (POD, owned by Rust, read by C++)
typedef struct DhModuleInfo {
    const char* name;
    const char* description;
    const char* license;
    const char* vermagic;
    const char** depends;
    size_t depends_count;
} DhModuleInfo;

// Symbol registry
DhSymbolRegistry* dh_symbol_registry_new(void);
void dh_symbol_registry_free(DhSymbolRegistry*);
void dh_symbol_registry_register(DhSymbolRegistry*, const char* name, void* addr);
void* dh_symbol_registry_resolve(const DhSymbolRegistry*, const char* name);
size_t dh_symbol_registry_size(const DhSymbolRegistry*);

// Module loader
DhModuleLoader* dh_module_loader_new(DhSymbolRegistry*);
void dh_module_loader_free(DhModuleLoader*);
DhLoadedModule* dh_module_loader_load(DhModuleLoader*, const char* name,
                                       const uint8_t* data, size_t size);

// Loaded module
void dh_loaded_module_free(DhLoadedModule*);
const DhModuleInfo* dh_loaded_module_info(const DhLoadedModule*);
int (*dh_loaded_module_init_fn(const DhLoadedModule*))(void);
void (*dh_loaded_module_exit_fn(const DhLoadedModule*))(void);

// Modinfo (standalone, no loader needed)
bool dh_extract_module_info(const uint8_t* data, size_t size, DhModuleInfo* out);
void dh_module_info_free(DhModuleInfo*);

// Dependency sort
bool dh_topological_sort(const char** names, const DhModuleInfo* infos,
                          size_t count, size_t* sorted_indices);

// DT provider
typedef struct DhDtEntry {
    const char* compatible;
    const char* module_path;
    size_t resource_count;
} DhDtEntry;

size_t dh_dt_parse_static(const uint8_t* dtb, size_t dtb_size,
                           DhDtEntry* entries, size_t max_entries);
```

## Build Integration

### Makefile (host development)

```makefile
RUST_LIB := rust/driverhub-core/target/release/libdriverhub_core.a

$(RUST_LIB):
	cd rust/driverhub-core && cargo build --release

$(TARGET): $(OBJS) $(RUST_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -lpthread
```

### GN (Fuchsia build)

```gn
rust_library("driverhub-core") {
  edition = "2021"
  source_root = "rust/driverhub-core/src/lib.rs"
  sources = [ ... ]
}
```

## Migration Steps

Each step is a separate commit:

1. Create `rust/driverhub-core/` crate with `Cargo.toml` and `src/lib.rs`
2. Port `modinfo_parser` → `modinfo.rs` with Rust tests
3. Port `dependency_sort` → `dependency_sort.rs` with Rust tests
4. Port `dt_provider` → `dt.rs` with Rust tests
5. Port `memory_allocator` + `mmap_allocator` → `allocator.rs`
6. Port `module_loader` → `loader.rs` + `relocations.rs` with Rust tests
7. Port `symbol_registry` → `symbols.rs`
8. Add FFI bridge (`lib.rs` exports + `driverhub_core.h`)
9. Update Makefile to build Rust lib and link
10. Update C++ callers to use FFI bridge
11. Remove replaced C++ files

## Testing Strategy

- Each Rust module has `#[cfg(test)] mod tests` matching C++ gtest coverage
- FFI integration tested by linking the Rust lib into the existing C++ tests
- Host `make test-all` continues to work throughout migration

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| FFI overhead on hot paths | Symbol resolution uses `HashMap` — same perf |
| Rust `object` crate too heavy | Use minimal ELF parsing by hand, like C++ does |
| Build complexity | Cargo build is one command; Makefile wraps it |
| `unsafe` in loader/allocator | Concentrated in two modules; rest is safe Rust |
