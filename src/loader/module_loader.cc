// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/module_loader.h"

#include <cstdio>
#include <cstring>
#include <elf.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/loader/memory_allocator.h"
#include "src/loader/modinfo_parser.h"
#include "src/symbols/symbol_registry.h"

// ELF ET_REL loader for Linux .ko kernel modules.
//
// This is a direct ELF parser rather than using elfldltl (which is
// Fuchsia-specific). The Fuchsia build would swap this for elfldltl.
// The logic is the same: parse sections, allocate, relocate, resolve symbols.
//
// Currently targets AArch64 (ARM64) relocations, the primary GKI target.

namespace driverhub {

namespace {

// Represents a section loaded into executable/writable memory.
struct LoadedSection {
  uint8_t* base = nullptr;
  size_t size = 0;
  size_t file_offset = 0;
  uint32_t type = 0;
  uint64_t flags = 0;
  std::string name;
};

}  // namespace

ModuleLoader::ModuleLoader(SymbolRegistry& symbols, MemoryAllocator& allocator)
    : symbols_(symbols), allocator_(allocator) {}
ModuleLoader::~ModuleLoader() = default;

std::unique_ptr<LoadedModule> ModuleLoader::Load(std::string_view name,
                                                  const uint8_t* data,
                                                  size_t size) {
  if (size < sizeof(Elf64_Ehdr)) {
    fprintf(stderr, "driverhub: %.*s: too small for ELF header\n",
            (int)name.size(), name.data());
    return nullptr;
  }

  auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);

  // Validate ELF magic.
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    fprintf(stderr, "driverhub: %.*s: bad ELF magic\n",
            (int)name.size(), name.data());
    return nullptr;
  }

  // Must be ET_REL (relocatable object) — .ko files are not ET_DYN.
  if (ehdr->e_type != ET_REL) {
    fprintf(stderr, "driverhub: %.*s: not ET_REL (type=%u)\n",
            (int)name.size(), name.data(), ehdr->e_type);
    return nullptr;
  }

  // We handle 64-bit ELF only (GKI is ARM64).
  if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
    fprintf(stderr, "driverhub: %.*s: not 64-bit ELF\n",
            (int)name.size(), name.data());
    return nullptr;
  }

  // Validate ELF machine type matches the running architecture.
#if defined(__aarch64__)
  constexpr uint16_t kExpectedMachine = EM_AARCH64;
  constexpr const char* kExpectedArch = "aarch64";
#elif defined(__x86_64__)
  constexpr uint16_t kExpectedMachine = EM_X86_64;
  constexpr const char* kExpectedArch = "x86_64";
#else
  constexpr uint16_t kExpectedMachine = 0;
  constexpr const char* kExpectedArch = "unknown";
#endif
  if (kExpectedMachine != 0 && ehdr->e_machine != kExpectedMachine) {
    fprintf(stderr,
            "driverhub: %.*s: wrong architecture (ELF machine=%u, "
            "expected %s/%u)\n",
            (int)name.size(), name.data(), ehdr->e_machine,
            kExpectedArch, kExpectedMachine);
    return nullptr;
  }

  // Parse section headers.
  if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) {
    fprintf(stderr, "driverhub: %.*s: no section headers\n",
            (int)name.size(), name.data());
    return nullptr;
  }

  auto* shdrs = reinterpret_cast<const Elf64_Shdr*>(data + ehdr->e_shoff);
  uint16_t shnum = ehdr->e_shnum;

  // Section name string table.
  const char* shstrtab = nullptr;
  if (ehdr->e_shstrndx < shnum) {
    shstrtab = reinterpret_cast<const char*>(
        data + shdrs[ehdr->e_shstrndx].sh_offset);
  }

  // First pass: calculate total allocation size and catalog sections.
  std::vector<LoadedSection> sections(shnum);
  size_t total_alloc = 0;

  for (uint16_t i = 0; i < shnum; i++) {
    const auto& shdr = shdrs[i];
    sections[i].type = shdr.sh_type;
    sections[i].flags = shdr.sh_flags;
    sections[i].file_offset = shdr.sh_offset;
    sections[i].size = shdr.sh_size;
    if (shstrtab && shdr.sh_name) {
      sections[i].name = shstrtab + shdr.sh_name;
    }

    if (shdr.sh_flags & SHF_ALLOC) {
      // Align to section alignment.
      size_t align = shdr.sh_addralign ? shdr.sh_addralign : 1;
      total_alloc = (total_alloc + align - 1) & ~(align - 1);
      total_alloc += shdr.sh_size;
    }
  }

#if defined(__x86_64__)
  // Count PC32/PLT32 relocations to estimate trampoline space needed.
  // Each trampoline is 14 bytes: FF 25 00 00 00 00 [8-byte abs addr].
  // We over-estimate (counting all, not just out-of-range) — the extra
  // space is negligible.
  size_t trampoline_count = 0;
  for (uint16_t i = 0; i < shnum; i++) {
    if (shdrs[i].sh_type != SHT_RELA) continue;
    auto* relas = reinterpret_cast<const Elf64_Rela*>(
        data + shdrs[i].sh_offset);
    size_t rela_count = shdrs[i].sh_size / sizeof(Elf64_Rela);
    for (size_t j = 0; j < rela_count; j++) {
      uint32_t rtype = ELF64_R_TYPE(relas[j].r_info);
      if (rtype == 2 || rtype == 4) {  // R_X86_64_PC32 or PLT32
        trampoline_count++;
      }
    }
  }
  constexpr size_t kTrampolineSize = 14;  // jmp [rip+0]; .quad addr
  size_t trampoline_space = trampoline_count * kTrampolineSize;
  // Align trampoline area to 16 bytes.
  total_alloc = (total_alloc + 15) & ~15UL;
  size_t trampoline_offset = total_alloc;
  total_alloc += trampoline_space;
#endif

  // Allocate RWX memory for all loadable sections via the platform allocator.
  // On host this uses mmap; on Fuchsia this uses VMOs.
  auto* alloc = allocator_.Allocate(total_alloc);
  if (!alloc) {
    fprintf(stderr, "driverhub: %.*s: memory allocation failed for %zu bytes\n",
            (int)name.size(), name.data(), total_alloc);
    return nullptr;
  }
  uint8_t* alloc_base = alloc->base;

  // Second pass: copy section data into the allocated region.
  size_t offset = 0;
  for (uint16_t i = 0; i < shnum; i++) {
    const auto& shdr = shdrs[i];
    if (!(shdr.sh_flags & SHF_ALLOC)) continue;

    size_t align = shdr.sh_addralign ? shdr.sh_addralign : 1;
    offset = (offset + align - 1) & ~(align - 1);

    sections[i].base = alloc_base + offset;

    if (shdr.sh_type != SHT_NOBITS) {
      memcpy(sections[i].base, data + shdr.sh_offset, shdr.sh_size);
    } else {
      // .bss — already zero from mmap.
    }

    offset += shdr.sh_size;
  }

  // Build local symbol table for this module.
  const Elf64_Sym* symtab = nullptr;
  size_t symcount = 0;
  const char* strtab = nullptr;

  for (uint16_t i = 0; i < shnum; i++) {
    if (shdrs[i].sh_type == SHT_SYMTAB) {
      symtab = reinterpret_cast<const Elf64_Sym*>(data + shdrs[i].sh_offset);
      symcount = shdrs[i].sh_size / sizeof(Elf64_Sym);
      // The associated string table.
      if (shdrs[i].sh_link < shnum) {
        strtab = reinterpret_cast<const char*>(
            data + shdrs[shdrs[i].sh_link].sh_offset);
      }
      break;
    }
  }

  if (!symtab || !strtab) {
    fprintf(stderr, "driverhub: %.*s: no symbol table found\n",
            (int)name.size(), name.data());
    alloc->Release();
    delete alloc;
    return nullptr;
  }

  // Resolve symbol addresses: local symbols get section-relative addresses,
  // undefined symbols are resolved against the KMI shim registry.
  std::vector<void*> sym_addrs(symcount, nullptr);

  for (size_t i = 0; i < symcount; i++) {
    const auto& sym = symtab[i];
    const char* sym_name = strtab + sym.st_name;

    if (sym.st_shndx == SHN_UNDEF) {
      if (sym.st_name == 0) continue;  // Null symbol.
      // Resolve against KMI shims and intermodule exports.
      void* addr = symbols_.Resolve(sym_name);
      if (!addr) {
        fprintf(stderr, "driverhub: %.*s: unresolved symbol: %s\n",
                (int)name.size(), name.data(), sym_name);
      }
      sym_addrs[i] = addr;
    } else if (sym.st_shndx < shnum) {
      // Defined symbol — compute address relative to loaded section.
      if (sections[sym.st_shndx].base) {
        sym_addrs[i] = sections[sym.st_shndx].base + sym.st_value;
      }
    } else if (sym.st_shndx == SHN_ABS) {
      sym_addrs[i] = reinterpret_cast<void*>(sym.st_value);
    }
  }

  // Apply relocations.
  for (uint16_t i = 0; i < shnum; i++) {
    if (shdrs[i].sh_type != SHT_RELA) continue;

    uint16_t target_sec = shdrs[i].sh_info;
    if (target_sec >= shnum || !sections[target_sec].base) continue;

    auto* relas = reinterpret_cast<const Elf64_Rela*>(
        data + shdrs[i].sh_offset);
    size_t rela_count = shdrs[i].sh_size / sizeof(Elf64_Rela);

    for (size_t j = 0; j < rela_count; j++) {
      const auto& rela = relas[j];
      uint32_t sym_idx = ELF64_R_SYM(rela.r_info);
      uint32_t rtype = ELF64_R_TYPE(rela.r_info);

      uint8_t* patch_addr = sections[target_sec].base + rela.r_offset;
      void* sym_val = (sym_idx < symcount) ? sym_addrs[sym_idx] : nullptr;
      uint64_t S = reinterpret_cast<uint64_t>(sym_val);
      uint64_t A = static_cast<uint64_t>(rela.r_addend);
      uint64_t P = reinterpret_cast<uint64_t>(patch_addr);

      // Handle architecture-specific relocation types.
      // This covers the common AArch64 and x86_64 relocation types.

#if defined(__aarch64__)
      // AArch64 relocation types.
      switch (rtype) {
        case 257:  // R_AARCH64_ABS64
          *reinterpret_cast<uint64_t*>(patch_addr) = S + A;
          break;
        case 258:  // R_AARCH64_ABS32
          *reinterpret_cast<uint32_t*>(patch_addr) =
              static_cast<uint32_t>(S + A);
          break;
        case 261:  // R_AARCH64_PREL32
          *reinterpret_cast<int32_t*>(patch_addr) =
              static_cast<int32_t>((S + A) - P);
          break;
        case 274:  // R_AARCH64_CALL26 / R_AARCH64_JUMP26
        case 283: {
          int64_t offset_val = static_cast<int64_t>((S + A) - P);
          uint32_t insn = *reinterpret_cast<uint32_t*>(patch_addr);
          insn = (insn & 0xFC000000u) |
                 (static_cast<uint32_t>((offset_val >> 2) & 0x03FFFFFF));
          *reinterpret_cast<uint32_t*>(patch_addr) = insn;
          break;
        }
        case 275:  // R_AARCH64_MOVW_UABS_G0_NC
        case 276:  // R_AARCH64_MOVW_UABS_G1_NC
        case 277:  // R_AARCH64_MOVW_UABS_G2_NC
        case 278: {  // R_AARCH64_MOVW_UABS_G3
          int shift = (rtype - 275) * 16;
          uint32_t insn = *reinterpret_cast<uint32_t*>(patch_addr);
          uint16_t val16 = static_cast<uint16_t>((S + A) >> shift);
          insn = (insn & ~(0xFFFFu << 5)) | (static_cast<uint32_t>(val16) << 5);
          *reinterpret_cast<uint32_t*>(patch_addr) = insn;
          break;
        }
        case 282: {  // R_AARCH64_ADR_PREL_PG_HI21
          int64_t page = ((S + A) & ~0xFFFULL) - (P & ~0xFFFULL);
          uint32_t insn = *reinterpret_cast<uint32_t*>(patch_addr);
          uint32_t immlo = static_cast<uint32_t>((page >> 12) & 0x3);
          uint32_t immhi = static_cast<uint32_t>((page >> 14) & 0x7FFFF);
          insn = (insn & 0x9F00001Fu) | (immlo << 29) | (immhi << 5);
          *reinterpret_cast<uint32_t*>(patch_addr) = insn;
          break;
        }
        case 286: {  // R_AARCH64_ADD_ABS_LO12_NC
          uint32_t insn = *reinterpret_cast<uint32_t*>(patch_addr);
          uint32_t imm12 = static_cast<uint32_t>((S + A) & 0xFFF);
          insn = (insn & ~(0xFFFu << 10)) | (imm12 << 10);
          *reinterpret_cast<uint32_t*>(patch_addr) = insn;
          break;
        }
        default:
          fprintf(stderr,
                  "driverhub: %.*s: unhandled AArch64 reloc type %u\n",
                  (int)name.size(), name.data(), rtype);
          break;
      }
#elif defined(__x86_64__)
      // x86_64 relocation types.
      switch (rtype) {
        case 1:  // R_X86_64_64
          *reinterpret_cast<uint64_t*>(patch_addr) = S + A;
          break;
        case 2:   // R_X86_64_PC32
        case 4: { // R_X86_64_PLT32
          int64_t rel_val = static_cast<int64_t>((S + A) - P);
          if (rel_val > INT32_MAX || rel_val < INT32_MIN) {
            // Target is >2GB away. Emit a trampoline in the module's
            // allocation (which IS within 32-bit range of the call site)
            // and redirect the call there.
            //
            // Trampoline: FF 25 00 00 00 00  jmp [rip+0]
            //             <8-byte abs addr>
            if (trampoline_offset + kTrampolineSize > alloc->size) {
              fprintf(stderr,
                      "driverhub: %.*s: trampoline space exhausted\n",
                      (int)name.size(), name.data());
              break;
            }
            // Build trampoline: jmp [rip+0]; .quad <absolute_target>
            // The trampoline target is S (the symbol's absolute address).
            // The addend A (typically -4) accounts for RIP advancement in
            // the original call/jmp instruction and must be included in the
            // relocation to the trampoline, not in the trampoline target.
            uint8_t* tramp = alloc_base + trampoline_offset;
            tramp[0] = 0xFF;
            tramp[1] = 0x25;
            tramp[2] = tramp[3] = tramp[4] = tramp[5] = 0;  // disp32=0
            memcpy(tramp + 6, &S, 8);  // absolute target = S

            // The relocation stores (target + A - P). Since the call/jmp
            // instruction computes branch_dest = disp32 + RIP = disp32 + P - A,
            // we need disp32 = T + A - P so that branch_dest = T.
            uint64_t T = reinterpret_cast<uint64_t>(tramp);
            int64_t tramp_rel = static_cast<int64_t>((T + A) - P);
            *reinterpret_cast<int32_t*>(patch_addr) =
                static_cast<int32_t>(tramp_rel);
            trampoline_offset += kTrampolineSize;
          } else {
            *reinterpret_cast<int32_t*>(patch_addr) =
                static_cast<int32_t>(rel_val);
          }
          break;
        }
        case 10:  // R_X86_64_32
          *reinterpret_cast<uint32_t*>(patch_addr) =
              static_cast<uint32_t>(S + A);
          break;
        case 11:  // R_X86_64_32S
          *reinterpret_cast<int32_t*>(patch_addr) =
              static_cast<int32_t>(S + A);
          break;
        default:
          fprintf(stderr,
                  "driverhub: %.*s: unhandled x86_64 reloc type %u\n",
                  (int)name.size(), name.data(), rtype);
          break;
      }
#else
      (void)S; (void)A; (void)P;
      fprintf(stderr,
              "driverhub: %.*s: relocations not supported on this arch\n",
              (int)name.size(), name.data());
      break;
#endif
    }
  }

  // Run constructors from .init_array / .ctors sections.
  // EXPORT_SYMBOL uses __attribute__((constructor)) which places function
  // pointers in .init_array.  These must run before init_module so that
  // exported symbols are registered.
  for (uint16_t i = 0; i < shnum; i++) {
    if (sections[i].name == ".init_array" && sections[i].base) {
      size_t count = sections[i].size / sizeof(void (*)());
      auto** ctors = reinterpret_cast<void (**)()>(sections[i].base);
      for (size_t c = 0; c < count; c++) {
        if (ctors[c]) {
          ctors[c]();
        }
      }
    }
  }

  // Build result.
  auto module = std::make_unique<LoadedModule>();
  module->name = std::string(name);

  // Extract module metadata from .modinfo section.
  for (uint16_t i = 0; i < shnum; i++) {
    if (sections[i].name == ".modinfo" && shdrs[i].sh_type != SHT_NOBITS) {
      ParseModinfo(data + shdrs[i].sh_offset, shdrs[i].sh_size,
                   &module->info);
      break;
    }
  }
  module->info.name = std::string(name);

  // Find init_module and cleanup_module entry points.
  for (size_t i = 0; i < symcount; i++) {
    const char* sym_name = strtab + symtab[i].st_name;
    if (strcmp(sym_name, "init_module") == 0) {
      module->init_fn = reinterpret_cast<int (*)()>(sym_addrs[i]);
    } else if (strcmp(sym_name, "cleanup_module") == 0) {
      module->exit_fn = reinterpret_cast<void (*)()>(sym_addrs[i]);
    }
  }

  // Transfer ownership of the allocation to the module.
  module->allocation.reset(alloc);

  fprintf(stderr,
          "driverhub: loaded %.*s (%zu bytes, %zu symbols, init=%p, exit=%p)\n",
          (int)name.size(), name.data(), size, symcount,
          reinterpret_cast<void*>(module->init_fn),
          reinterpret_cast<void*>(module->exit_fn));

  return module;
}

}  // namespace driverhub
