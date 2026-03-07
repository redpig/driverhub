// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runner/process_manager.h"

#include <dl/dl.h>
#include <elf.h>
#include <fcntl.h>
#include <lib/async/cpp/wait.h>
#include <lib/fdio/io.h>
#include <lib/zx/job.h>
#include <lib/zx/thread.h>
#include <link.h>
#include <zircon/process.h>
#include <zircon/processargs.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>

#include <cstdio>
#include <cstring>

#include "src/runner/symbol_registry_server.h"

namespace driverhub {

namespace {

constexpr uint64_t kPageSize = 4096;

// Align a value up to the given alignment.
uint64_t AlignUp(uint64_t value, uint64_t align) {
  return (value + align - 1) & ~(align - 1);
}

// Round down to page boundary.
uint64_t PageFloor(uint64_t addr) { return addr & ~(kPageSize - 1); }

// Round up to page boundary.
uint64_t PageCeil(uint64_t size) { return AlignUp(size, kPageSize); }

// Map the vDSO into a child process's VMAR.
//
// The vDSO is required for any Zircon syscalls in the child process. We find
// it by iterating the runner's own program headers (the vDSO is always mapped
// into every process by the kernel), then map the same VMO into the child.
zx_status_t MapVdsoIntoChild(const zx::vmar& child_vmar) {
  // Find the vDSO VMO. On Fuchsia, the vDSO is the first (and only) VMO in
  // the processargs bootstrap with type PA_VMO_VDSO. We can also find it by
  // looking at our own process's mapped regions for the "[vdso]" region and
  // getting a VMO handle for it.
  //
  // The canonical way is via dl_iterate_phdr to find the vDSO base, then
  // use zx_vmo_create_child to get a handle. But the simplest approach is
  // to use the zx_process_get_vdso_base() bootstrap handle if available.
  //
  // For robustness, we find it via dl_iterate_phdr which always works.
  struct VdsoInfo {
    uintptr_t base = 0;
    size_t size = 0;
    bool found = false;
  };

  VdsoInfo info;
  dl_iterate_phdr(
      [](struct dl_phdr_info* phdr_info, size_t, void* data) -> int {
        auto* vinfo = static_cast<VdsoInfo*>(data);
        // The vDSO has no name (empty string) and is read-only + executable.
        if (phdr_info->dlpi_name && phdr_info->dlpi_name[0] == '\0') {
          // Check if this looks like the vDSO by looking for PT_LOAD segments.
          for (int i = 0; i < phdr_info->dlpi_phnum; i++) {
            if (phdr_info->dlpi_phdr[i].p_type == PT_LOAD) {
              if (!vinfo->found) {
                vinfo->base = phdr_info->dlpi_addr;
                vinfo->found = true;
              }
              uint64_t seg_end =
                  phdr_info->dlpi_phdr[i].p_vaddr +
                  phdr_info->dlpi_phdr[i].p_memsz;
              if (seg_end > vinfo->size) {
                vinfo->size = seg_end;
              }
            }
          }
        }
        return 0;
      },
      &info);

  if (!info.found || info.size == 0) {
    fprintf(stderr, "driverhub: process_manager: vDSO not found\n");
    return ZX_ERR_NOT_FOUND;
  }

  size_t vdso_size = PageCeil(info.size);

  // Create a VMO from the vDSO memory region. The kernel provides the vDSO
  // as a read-only VMO that can be mapped RX into any process.
  //
  // We get the vDSO VMO handle via zx_take_startup_handle(PA_HND(PA_VMO_VDSO,
  // 0)) which was provided at process start. Since we may have already
  // consumed it, fall back to creating a VMO clone from the mapped region.
  zx::vmo vdso_vmo;

  // Use the process self handle to look up the vDSO VMO from our own mappings.
  // On Fuchsia, the vDSO VMO can be obtained from the process bootstrap or
  // from the global __zx_vdso_vmo (if the runtime exposes it).
  //
  // Alternative approach: create a VMO and copy the vDSO contents into it.
  zx_status_t status = zx::vmo::create(vdso_size, 0, &vdso_vmo);
  if (status != ZX_OK) return status;

  // Copy the vDSO contents from our own address space into the VMO.
  status = vdso_vmo.write(reinterpret_cast<const void*>(info.base), 0,
                          vdso_size);
  if (status != ZX_OK) return status;

  // Make the VMO executable so it can be mapped RX in the child.
  status = vdso_vmo.replace_as_executable(zx::resource(), &vdso_vmo);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: vDSO replace_as_executable "
            "failed: %s\n",
            zx_status_get_string(status));
    return status;
  }

  vdso_vmo.set_property(ZX_PROP_NAME, "vdso", 4);

  // Map the vDSO into the child's address space.
  zx_vaddr_t child_vdso_addr = 0;
  status = child_vmar.map(ZX_VM_PERM_READ | ZX_VM_PERM_EXECUTE, 0,
                          vdso_vmo, 0, vdso_size, &child_vdso_addr);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: vDSO map into child failed: %s\n",
            zx_status_get_string(status));
    return status;
  }

  fprintf(stderr,
          "driverhub: process_manager: vDSO mapped into child at 0x%lx "
          "(%zu bytes)\n",
          child_vdso_addr, vdso_size);

  return ZX_OK;
}

}  // namespace

// ---------------------------------------------------------------------------
// ManagedProcess
// ---------------------------------------------------------------------------

ManagedProcess::ManagedProcess(std::string name, zx::process process,
                               zx::vmar root_vmar)
    : name_(std::move(name)),
      process_(std::move(process)),
      root_vmar_(std::move(root_vmar)) {}

ManagedProcess::~ManagedProcess() {
  if (process_.is_valid()) {
    process_.kill();
  }
}

// ---------------------------------------------------------------------------
// InjectShimLibrary — map libdriverhub_kmi.so into the child process
// ---------------------------------------------------------------------------

zx_status_t ManagedProcess::InjectShimLibrary() {
  // Map the vDSO into the child process first. The vDSO provides the
  // syscall stubs that all Zircon userspace code needs.
  zx_status_t vdso_status = MapVdsoIntoChild(root_vmar_);
  if (vdso_status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: warning: vDSO injection "
            "failed: %s (child syscalls will not work)\n",
            zx_status_get_string(vdso_status));
    // Continue anyway — the vDSO mapping may not be available in test
    // environments but we still want to load the shim library.
  }

  // Open the runner's packaged copy of the KMI shim shared library.
  int fd = open("/pkg/lib/libdriverhub_kmi.so", O_RDONLY);
  if (fd < 0) {
    fprintf(stderr,
            "driverhub: process_manager: failed to open "
            "libdriverhub_kmi.so: %s\n",
            strerror(errno));
    return ZX_ERR_NOT_FOUND;
  }

  // Get a VMO clone of the file contents.
  zx::vmo file_vmo;
  zx_status_t status =
      fdio_get_vmo_clone(fd, file_vmo.reset_and_get_address());
  close(fd);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: fdio_get_vmo_clone failed: %s\n",
            zx_status_get_string(status));
    return status;
  }

  // Read and validate the ELF header.
  Elf64_Ehdr ehdr;
  status = file_vmo.read(&ehdr, 0, sizeof(ehdr));
  if (status != ZX_OK) return status;

  if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
      ehdr.e_ident[EI_CLASS] != ELFCLASS64 || ehdr.e_type != ET_DYN) {
    fprintf(stderr,
            "driverhub: process_manager: shim library is not a valid "
            "64-bit shared object\n");
    return ZX_ERR_BAD_STATE;
  }

  // Read program headers.
  std::vector<Elf64_Phdr> phdrs(ehdr.e_phnum);
  status = file_vmo.read(phdrs.data(), ehdr.e_phoff,
                         ehdr.e_phnum * sizeof(Elf64_Phdr));
  if (status != ZX_OK) return status;

  // Calculate the total virtual address span from PT_LOAD segments.
  uint64_t vaddr_min = UINT64_MAX;
  uint64_t vaddr_max = 0;
  for (const auto& phdr : phdrs) {
    if (phdr.p_type != PT_LOAD) continue;
    uint64_t seg_start = PageFloor(phdr.p_vaddr);
    uint64_t seg_end = PageCeil(phdr.p_vaddr + phdr.p_memsz);
    if (seg_start < vaddr_min) vaddr_min = seg_start;
    if (seg_end > vaddr_max) vaddr_max = seg_end;
  }

  if (vaddr_min >= vaddr_max) {
    fprintf(stderr,
            "driverhub: process_manager: no PT_LOAD segments in shim\n");
    return ZX_ERR_BAD_STATE;
  }

  size_t total_size = vaddr_max - vaddr_min;

  // Allocate a sub-VMAR in the child process for the entire library span.
  // Use ZX_VM_OFFSET_IS_UPPER_LIMIT to place in low address space for
  // -mcmodel=small compatibility (addresses must fit in 32-bit signed).
  zx_vaddr_t child_region_base = 0;
  zx::vmar child_sub_vmar;
  constexpr uint64_t kLow2GB = 0x80000000ULL;

  status = root_vmar_.allocate(
      ZX_VM_CAN_MAP_READ | ZX_VM_CAN_MAP_WRITE | ZX_VM_CAN_MAP_EXECUTE |
          ZX_VM_CAN_MAP_SPECIFIC | ZX_VM_OFFSET_IS_UPPER_LIMIT,
      kLow2GB, total_size, &child_sub_vmar, &child_region_base);
  if (status != ZX_OK) {
    // Fall back to default placement if low-address allocation fails.
    status = root_vmar_.allocate(
        ZX_VM_CAN_MAP_READ | ZX_VM_CAN_MAP_WRITE | ZX_VM_CAN_MAP_EXECUTE |
            ZX_VM_CAN_MAP_SPECIFIC,
        0, total_size, &child_sub_vmar, &child_region_base);
    if (status != ZX_OK) {
      fprintf(stderr,
              "driverhub: process_manager: VMAR allocate for shim "
              "failed: %s\n",
              zx_status_get_string(status));
      return status;
    }
  }

  shim_base_ = child_region_base;

  // Map each PT_LOAD segment into the child's sub-VMAR.
  for (const auto& phdr : phdrs) {
    if (phdr.p_type != PT_LOAD) continue;

    uint64_t seg_page_start = PageFloor(phdr.p_vaddr);
    uint64_t page_offset_in_file =
        phdr.p_offset - (phdr.p_vaddr - seg_page_start);
    size_t seg_page_size = PageCeil(phdr.p_vaddr + phdr.p_memsz) -
                           seg_page_start;

    // Create a VMO for this segment and copy data into it.
    zx::vmo seg_vmo;
    status = zx::vmo::create(seg_page_size, 0, &seg_vmo);
    if (status != ZX_OK) return status;

    // Copy file contents for the filesz portion.
    if (phdr.p_filesz > 0) {
      // Read from file VMO in chunks to handle large segments.
      constexpr size_t kChunkSize = 64 * 1024;
      size_t page_padding = phdr.p_vaddr - seg_page_start;
      size_t remaining = phdr.p_filesz;
      size_t src_off = phdr.p_offset;
      size_t dst_off = page_padding;
      std::vector<uint8_t> buf(kChunkSize);

      while (remaining > 0) {
        size_t chunk = std::min(remaining, kChunkSize);
        status = file_vmo.read(buf.data(), src_off, chunk);
        if (status != ZX_OK) return status;
        status = seg_vmo.write(buf.data(), dst_off, chunk);
        if (status != ZX_OK) return status;
        src_off += chunk;
        dst_off += chunk;
        remaining -= chunk;
      }
    }

    // Determine mapping permissions.
    zx_vm_option_t perms = ZX_VM_SPECIFIC;
    if (phdr.p_flags & PF_R) perms |= ZX_VM_PERM_READ;
    if (phdr.p_flags & PF_W) perms |= ZX_VM_PERM_WRITE;
    if (phdr.p_flags & PF_X) {
      status = seg_vmo.replace_as_executable(zx::resource(), &seg_vmo);
      if (status != ZX_OK) {
        fprintf(stderr,
                "driverhub: process_manager: replace_as_executable "
                "for shim segment failed: %s\n",
                zx_status_get_string(status));
        return status;
      }
      perms |= ZX_VM_PERM_EXECUTE;
    }

    // Map at the correct offset within the sub-VMAR.
    uint64_t vmar_offset = seg_page_start - vaddr_min;
    zx_vaddr_t mapped_addr;
    status = child_sub_vmar.map(perms, vmar_offset, seg_vmo, 0,
                                seg_page_size, &mapped_addr);
    if (status != ZX_OK) {
      fprintf(stderr,
              "driverhub: process_manager: shim segment map failed: %s\n",
              zx_status_get_string(status));
      return status;
    }

    child_mappings_.push_back({std::move(seg_vmo), mapped_addr, seg_page_size});
  }

  // Apply R_*_RELATIVE relocations so the library works at its loaded address.
  // These are found in the PT_DYNAMIC segment's DT_RELA/DT_RELASZ entries,
  // or we can scan section headers for .rela.dyn.
  for (const auto& phdr : phdrs) {
    if (phdr.p_type != PT_DYNAMIC) continue;

    // Read the dynamic section entries.
    size_t dyn_count = phdr.p_filesz / sizeof(Elf64_Dyn);
    std::vector<Elf64_Dyn> dyns(dyn_count);
    status = file_vmo.read(dyns.data(), phdr.p_offset, phdr.p_filesz);
    if (status != ZX_OK) break;

    uint64_t rela_offset = 0;
    uint64_t rela_size = 0;
    for (const auto& dyn : dyns) {
      if (dyn.d_tag == DT_RELA)
        rela_offset = dyn.d_un.d_ptr;
      else if (dyn.d_tag == DT_RELASZ)
        rela_size = dyn.d_un.d_val;
    }

    if (rela_offset == 0 || rela_size == 0) break;

    // rela_offset is a virtual address in the library's address space.
    // Convert to file offset by finding which PT_LOAD contains it.
    uint64_t rela_file_offset = 0;
    for (const auto& lphdr : phdrs) {
      if (lphdr.p_type != PT_LOAD) continue;
      if (rela_offset >= lphdr.p_vaddr &&
          rela_offset < lphdr.p_vaddr + lphdr.p_filesz) {
        rela_file_offset =
            lphdr.p_offset + (rela_offset - lphdr.p_vaddr);
        break;
      }
    }
    if (rela_file_offset == 0) break;

    size_t rela_count = rela_size / sizeof(Elf64_Rela);
    std::vector<Elf64_Rela> relas(rela_count);
    status = file_vmo.read(relas.data(), rela_file_offset, rela_size);
    if (status != ZX_OK) break;

    // The load bias: difference between actual load address and link address.
    int64_t bias = static_cast<int64_t>(shim_base_) -
                   static_cast<int64_t>(vaddr_min);

    for (const auto& rela : relas) {
      uint32_t rtype = ELF64_R_TYPE(rela.r_info);

      bool is_relative = false;
#if defined(__aarch64__)
      is_relative = (rtype == 1027);  // R_AARCH64_RELATIVE
#elif defined(__x86_64__)
      is_relative = (rtype == 8);  // R_X86_64_RELATIVE
#endif

      if (!is_relative) continue;

      // R_*_RELATIVE: *patch = base + addend
      uint64_t patch_vaddr = rela.r_offset + bias + vaddr_min;
      uint64_t value = shim_base_ + rela.r_addend;

      // Write the patched value through the backing VMO.
      // Find which child mapping contains this address.
      for (auto& mapping : child_mappings_) {
        if (patch_vaddr >= mapping.addr &&
            patch_vaddr + sizeof(uint64_t) <= mapping.addr + mapping.size) {
          uint64_t vmo_offset = patch_vaddr - mapping.addr;
          mapping.vmo.write(&value, vmo_offset, sizeof(value));
          break;
        }
      }
    }
    break;  // Only one PT_DYNAMIC.
  }

  // Parse .dynsym and .dynstr to extract exported symbol addresses.
  if (ehdr.e_shnum > 0 && ehdr.e_shstrndx < ehdr.e_shnum) {
    std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
    status = file_vmo.read(shdrs.data(), ehdr.e_shoff,
                           ehdr.e_shnum * sizeof(Elf64_Shdr));
    if (status == ZX_OK) {
      // Read section name string table.
      const Elf64_Shdr& shstrtab_hdr = shdrs[ehdr.e_shstrndx];
      std::vector<char> shstrtab(shstrtab_hdr.sh_size);
      file_vmo.read(shstrtab.data(), shstrtab_hdr.sh_offset,
                    shstrtab_hdr.sh_size);

      // Find .dynsym and .dynstr sections.
      const Elf64_Shdr* dynsym_hdr = nullptr;
      const Elf64_Shdr* dynstr_hdr = nullptr;
      for (uint16_t i = 0; i < ehdr.e_shnum; i++) {
        if (shdrs[i].sh_name >= shstrtab.size()) continue;
        const char* sec_name = shstrtab.data() + shdrs[i].sh_name;
        if (strcmp(sec_name, ".dynsym") == 0) dynsym_hdr = &shdrs[i];
        if (strcmp(sec_name, ".dynstr") == 0) dynstr_hdr = &shdrs[i];
      }

      if (dynsym_hdr && dynstr_hdr) {
        size_t sym_count = dynsym_hdr->sh_size / sizeof(Elf64_Sym);
        std::vector<Elf64_Sym> dynsyms(sym_count);
        file_vmo.read(dynsyms.data(), dynsym_hdr->sh_offset,
                      dynsym_hdr->sh_size);

        std::vector<char> dynstr(dynstr_hdr->sh_size);
        file_vmo.read(dynstr.data(), dynstr_hdr->sh_offset,
                      dynstr_hdr->sh_size);

        for (size_t i = 0; i < sym_count; i++) {
          const auto& sym = dynsyms[i];
          if (sym.st_name == 0 || sym.st_shndx == SHN_UNDEF) continue;
          if (ELF64_ST_BIND(sym.st_info) != STB_GLOBAL &&
              ELF64_ST_BIND(sym.st_info) != STB_WEAK)
            continue;
          if (sym.st_name >= dynstr.size()) continue;

          const char* sym_name = dynstr.data() + sym.st_name;
          // For ET_DYN, symbol value is relative to load base.
          uint64_t sym_addr = shim_base_ + sym.st_value;
          shim_symbols_[sym_name] = sym_addr;
        }
      }
    }
  }

  fprintf(stderr,
          "driverhub: process_manager: loaded shim library into %s "
          "at 0x%lx (%zu bytes, %zu KMI symbols)\n",
          name_.c_str(), shim_base_, total_size, shim_symbols_.size());

  return ZX_OK;
}

// ---------------------------------------------------------------------------
// ReadModuleBinary — read .ko from the component's /pkg namespace
// ---------------------------------------------------------------------------

zx_status_t ManagedProcess::ReadModuleBinary(
    const fuchsia_component_runner::ComponentStartInfo& start_info,
    const std::string& ko_path,
    std::vector<uint8_t>* out) {
  if (!start_info.ns().has_value()) {
    fprintf(stderr,
            "driverhub: process_manager: no namespace in start_info\n");
    return ZX_ERR_NOT_FOUND;
  }

  for (const auto& entry : start_info.ns().value()) {
    if (!entry.path().has_value() || entry.path().value() != "/pkg") {
      continue;
    }

    if (!entry.directory().has_value()) {
      return ZX_ERR_NOT_FOUND;
    }

    // Duplicate the /pkg directory channel and convert it to a file descriptor
    // so we can use POSIX openat() to read the .ko file.
    auto& dir_client = entry.directory().value();
    zx::channel dir_chan;
    zx_status_t status =
        dir_client.channel()->duplicate(ZX_RIGHT_SAME_RIGHTS, &dir_chan);
    if (status != ZX_OK) {
      fprintf(stderr,
              "driverhub: process_manager: failed to duplicate /pkg "
              "channel: %s\n",
              zx_status_get_string(status));
      return status;
    }

    int dir_fd = -1;
    status = fdio_fd_create(dir_chan.release(), &dir_fd);
    if (status != ZX_OK) {
      fprintf(stderr,
              "driverhub: process_manager: fdio_fd_create failed: %s\n",
              zx_status_get_string(status));
      return status;
    }

    // Open the .ko file relative to /pkg.
    int file_fd = openat(dir_fd, ko_path.c_str(), O_RDONLY);
    if (file_fd < 0) {
      fprintf(stderr,
              "driverhub: process_manager: failed to open %s: %s\n",
              ko_path.c_str(), strerror(errno));
      close(dir_fd);
      return ZX_ERR_NOT_FOUND;
    }

    // Get the file size.
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    if (file_size <= 0) {
      close(file_fd);
      close(dir_fd);
      return ZX_ERR_IO;
    }
    lseek(file_fd, 0, SEEK_SET);

    // Read the entire file.
    out->resize(static_cast<size_t>(file_size));
    size_t total_read = 0;
    while (total_read < out->size()) {
      ssize_t n = read(file_fd, out->data() + total_read,
                       out->size() - total_read);
      if (n <= 0) {
        if (n < 0 && errno == EINTR) continue;
        close(file_fd);
        close(dir_fd);
        return ZX_ERR_IO;
      }
      total_read += static_cast<size_t>(n);
    }

    close(file_fd);
    close(dir_fd);

    fprintf(stderr,
            "driverhub: process_manager: read %s (%zu bytes)\n",
            ko_path.c_str(), out->size());
    return ZX_OK;
  }

  fprintf(stderr,
          "driverhub: process_manager: /pkg namespace entry not found\n");
  return ZX_ERR_NOT_FOUND;
}

// ---------------------------------------------------------------------------
// LoadModule — cross-VMAR ELF ET_REL loading
// ---------------------------------------------------------------------------

zx_status_t ManagedProcess::LoadModule(const uint8_t* data, size_t size,
                                        SymbolRegistryServer& registry) {
  if (size < sizeof(Elf64_Ehdr)) {
    fprintf(stderr, "driverhub: process_manager: module too small\n");
    return ZX_ERR_INVALID_ARGS;
  }

  auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    fprintf(stderr, "driverhub: process_manager: bad ELF magic\n");
    return ZX_ERR_INVALID_ARGS;
  }
  if (ehdr->e_type != ET_REL) {
    fprintf(stderr, "driverhub: process_manager: not ET_REL\n");
    return ZX_ERR_INVALID_ARGS;
  }
  if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
    fprintf(stderr, "driverhub: process_manager: not 64-bit ELF\n");
    return ZX_ERR_INVALID_ARGS;
  }
  if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) {
    fprintf(stderr, "driverhub: process_manager: no section headers\n");
    return ZX_ERR_INVALID_ARGS;
  }

  auto* shdrs = reinterpret_cast<const Elf64_Shdr*>(data + ehdr->e_shoff);
  uint16_t shnum = ehdr->e_shnum;

  // Section name string table.
  const char* shstrtab = nullptr;
  if (ehdr->e_shstrndx < shnum) {
    shstrtab = reinterpret_cast<const char*>(
        data + shdrs[ehdr->e_shstrndx].sh_offset);
  }

  // First pass: calculate total allocation size for SHF_ALLOC sections.
  struct SectionInfo {
    size_t alloc_offset = 0;  // Offset within the allocation.
    size_t size = 0;
    uint32_t type = 0;
    uint64_t flags = 0;
    std::string name;
  };
  std::vector<SectionInfo> sections(shnum);
  size_t total_alloc = 0;

  for (uint16_t i = 0; i < shnum; i++) {
    const auto& shdr = shdrs[i];
    sections[i].type = shdr.sh_type;
    sections[i].flags = shdr.sh_flags;
    sections[i].size = shdr.sh_size;
    if (shstrtab && shdr.sh_name) {
      sections[i].name = shstrtab + shdr.sh_name;
    }

    if (shdr.sh_flags & SHF_ALLOC) {
      size_t align = shdr.sh_addralign ? shdr.sh_addralign : 1;
      total_alloc = AlignUp(total_alloc, align);
      sections[i].alloc_offset = total_alloc;
      total_alloc += shdr.sh_size;
    }
  }

  total_alloc = PageCeil(total_alloc);

  // Create a VMO for the module's sections.
  zx::vmo module_vmo;
  zx_status_t status = zx::vmo::create(total_alloc, 0, &module_vmo);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: VMO create failed: %s\n",
            zx_status_get_string(status));
    return status;
  }
  module_vmo.set_property(ZX_PROP_NAME, "driverhub-ko", 12);

  // Write section data into the VMO.
  for (uint16_t i = 0; i < shnum; i++) {
    const auto& shdr = shdrs[i];
    if (!(shdr.sh_flags & SHF_ALLOC)) continue;
    if (shdr.sh_type == SHT_NOBITS) continue;  // .bss — VMO is zeroed.

    status = module_vmo.write(data + shdr.sh_offset,
                              sections[i].alloc_offset, shdr.sh_size);
    if (status != ZX_OK) {
      fprintf(stderr,
              "driverhub: process_manager: VMO write for section %s "
              "failed: %s\n",
              sections[i].name.c_str(), zx_status_get_string(status));
      return status;
    }
  }

  // Map the VMO into the child's VMAR to determine the base address.
  // Try low address space first for -mcmodel=small compatibility.
  status = module_vmo.replace_as_executable(zx::resource(), &module_vmo);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: replace_as_executable failed: %s\n",
            zx_status_get_string(status));
    return status;
  }

  zx_vaddr_t child_base = 0;
  constexpr uint64_t kLow2GB = 0x80000000ULL;
  status = root_vmar_.map(
      ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE |
          ZX_VM_OFFSET_IS_UPPER_LIMIT,
      kLow2GB, module_vmo, 0, total_alloc, &child_base);
  if (status != ZX_OK) {
    // Fall back to default placement.
    status = root_vmar_.map(
        ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE,
        0, module_vmo, 0, total_alloc, &child_base);
    if (status != ZX_OK) {
      fprintf(stderr,
              "driverhub: process_manager: VMAR map for module "
              "failed: %s\n",
              zx_status_get_string(status));
      return status;
    }
  }

  // Build local symbol table.
  const Elf64_Sym* symtab = nullptr;
  size_t symcount = 0;
  const char* strtab = nullptr;

  for (uint16_t i = 0; i < shnum; i++) {
    if (shdrs[i].sh_type == SHT_SYMTAB) {
      symtab = reinterpret_cast<const Elf64_Sym*>(data + shdrs[i].sh_offset);
      symcount = shdrs[i].sh_size / sizeof(Elf64_Sym);
      if (shdrs[i].sh_link < shnum) {
        strtab = reinterpret_cast<const char*>(
            data + shdrs[shdrs[i].sh_link].sh_offset);
      }
      break;
    }
  }

  if (!symtab || !strtab) {
    fprintf(stderr,
            "driverhub: process_manager: no symbol table in module\n");
    return ZX_ERR_BAD_STATE;
  }

  // Resolve symbol addresses.
  // Local symbols: child_base + section offset + sym value.
  // Undefined symbols: look up in shim_symbols_ first, then SymbolRegistry.
  std::vector<uint64_t> sym_addrs(symcount, 0);

  for (size_t i = 0; i < symcount; i++) {
    const auto& sym = symtab[i];
    const char* sym_name = strtab + sym.st_name;

    if (sym.st_shndx == SHN_UNDEF) {
      if (sym.st_name == 0) continue;

      // Try the shim library first (KMI symbols).
      auto it = shim_symbols_.find(sym_name);
      if (it != shim_symbols_.end()) {
        sym_addrs[i] = it->second;
        continue;
      }

      // Try the cross-module SymbolRegistry (EXPORT_SYMBOL from other .ko).
      auto lookup = registry.LookupDirect(sym_name);
      if (lookup.found) {
        if (lookup.kind == fuchsia_driverhub::SymbolKind::kData &&
            lookup.vmo) {
          // DATA symbol: map the exporting module's VMO into this process
          // and compute the symbol address from offset.
          zx::vmo dup_vmo;
          zx_status_t dup_status =
              lookup.vmo->duplicate(ZX_RIGHT_SAME_RIGHTS, &dup_vmo);
          if (dup_status == ZX_OK) {
            uint64_t vmo_size = 0;
            dup_vmo.get_size(&vmo_size);
            zx_vaddr_t data_addr = 0;
            dup_status = root_vmar_.map(ZX_VM_PERM_READ | ZX_VM_PERM_WRITE,
                                        0, dup_vmo, 0, vmo_size, &data_addr);
            if (dup_status == ZX_OK) {
              sym_addrs[i] = data_addr + lookup.offset;
              child_mappings_.push_back(
                  {std::move(dup_vmo), data_addr, vmo_size});
              continue;
            }
          }
          fprintf(stderr,
                  "driverhub: process_manager: failed to map DATA "
                  "symbol %s: %s\n",
                  sym_name, zx_status_get_string(dup_status));
        } else if (lookup.virtual_addr != 0) {
          // Colocated module: symbol is in the same address space.
          sym_addrs[i] = lookup.virtual_addr;
          continue;
        }
        // FUNCTION symbols from non-colocated modules require a
        // trampoline stub in the child that marshals calls over a
        // SymbolProxy channel. This needs architecture-specific code
        // generation and is deferred to a future phase.
      }

      if (sym_addrs[i] == 0) {
        fprintf(stderr,
                "driverhub: process_manager: unresolved symbol: %s\n",
                sym_name);
      }
    } else if (sym.st_shndx < shnum) {
      if (sections[sym.st_shndx].flags & SHF_ALLOC) {
        sym_addrs[i] =
            child_base + sections[sym.st_shndx].alloc_offset + sym.st_value;
      }
    } else if (sym.st_shndx == SHN_ABS) {
      sym_addrs[i] = sym.st_value;
    }
  }

  // Apply relocations via VMO read/write.
  // All addresses are computed relative to child_base (the address in the
  // child process's address space).
  for (uint16_t i = 0; i < shnum; i++) {
    if (shdrs[i].sh_type != SHT_RELA) continue;

    uint16_t target_sec = shdrs[i].sh_info;
    if (target_sec >= shnum || !(sections[target_sec].flags & SHF_ALLOC))
      continue;

    auto* relas =
        reinterpret_cast<const Elf64_Rela*>(data + shdrs[i].sh_offset);
    size_t rela_count = shdrs[i].sh_size / sizeof(Elf64_Rela);

    for (size_t j = 0; j < rela_count; j++) {
      const auto& rela = relas[j];
      uint32_t sym_idx = ELF64_R_SYM(rela.r_info);
      uint32_t rtype = ELF64_R_TYPE(rela.r_info);

      // Patch location in the child's address space.
      uint64_t P = child_base + sections[target_sec].alloc_offset +
                   rela.r_offset;
      // VMO offset for the patch location.
      uint64_t vmo_patch_offset =
          sections[target_sec].alloc_offset + rela.r_offset;

      uint64_t S = (sym_idx < symcount) ? sym_addrs[sym_idx] : 0;
      int64_t A = rela.r_addend;

#if defined(__aarch64__)
      switch (rtype) {
        case 257: {  // R_AARCH64_ABS64
          uint64_t val = S + A;
          module_vmo.write(&val, vmo_patch_offset, sizeof(val));
          break;
        }
        case 258: {  // R_AARCH64_ABS32
          uint32_t val = static_cast<uint32_t>(S + A);
          module_vmo.write(&val, vmo_patch_offset, sizeof(val));
          break;
        }
        case 261: {  // R_AARCH64_PREL32
          int32_t val = static_cast<int32_t>((S + A) - P);
          module_vmo.write(&val, vmo_patch_offset, sizeof(val));
          break;
        }
        case 274:    // R_AARCH64_CALL26
        case 283: {  // R_AARCH64_JUMP26
          int64_t offset_val = static_cast<int64_t>((S + A) - P);
          uint32_t insn;
          module_vmo.read(&insn, vmo_patch_offset, sizeof(insn));
          insn = (insn & 0xFC000000u) |
                 (static_cast<uint32_t>((offset_val >> 2) & 0x03FFFFFF));
          module_vmo.write(&insn, vmo_patch_offset, sizeof(insn));
          break;
        }
        case 275:    // R_AARCH64_MOVW_UABS_G0_NC
        case 276:    // R_AARCH64_MOVW_UABS_G1_NC
        case 277:    // R_AARCH64_MOVW_UABS_G2_NC
        case 278: {  // R_AARCH64_MOVW_UABS_G3
          int shift = (rtype - 275) * 16;
          uint32_t insn;
          module_vmo.read(&insn, vmo_patch_offset, sizeof(insn));
          uint16_t val16 = static_cast<uint16_t>((S + A) >> shift);
          insn = (insn & ~(0xFFFFu << 5)) |
                 (static_cast<uint32_t>(val16) << 5);
          module_vmo.write(&insn, vmo_patch_offset, sizeof(insn));
          break;
        }
        case 282: {  // R_AARCH64_ADR_PREL_PG_HI21
          int64_t page =
              static_cast<int64_t>(((S + A) & ~0xFFFULL) - (P & ~0xFFFULL));
          uint32_t insn;
          module_vmo.read(&insn, vmo_patch_offset, sizeof(insn));
          uint32_t immlo = static_cast<uint32_t>((page >> 12) & 0x3);
          uint32_t immhi = static_cast<uint32_t>((page >> 14) & 0x7FFFF);
          insn = (insn & 0x9F00001Fu) | (immlo << 29) | (immhi << 5);
          module_vmo.write(&insn, vmo_patch_offset, sizeof(insn));
          break;
        }
        case 286: {  // R_AARCH64_ADD_ABS_LO12_NC
          uint32_t insn;
          module_vmo.read(&insn, vmo_patch_offset, sizeof(insn));
          uint32_t imm12 = static_cast<uint32_t>((S + A) & 0xFFF);
          insn = (insn & ~(0xFFFu << 10)) | (imm12 << 10);
          module_vmo.write(&insn, vmo_patch_offset, sizeof(insn));
          break;
        }
        default:
          fprintf(stderr,
                  "driverhub: process_manager: unhandled AArch64 "
                  "reloc type %u\n",
                  rtype);
          break;
      }
#elif defined(__x86_64__)
      switch (rtype) {
        case 1: {  // R_X86_64_64
          uint64_t val = S + A;
          module_vmo.write(&val, vmo_patch_offset, sizeof(val));
          break;
        }
        case 2:    // R_X86_64_PC32
        case 4: {  // R_X86_64_PLT32
          int32_t val = static_cast<int32_t>((S + A) - P);
          module_vmo.write(&val, vmo_patch_offset, sizeof(val));
          break;
        }
        case 10: {  // R_X86_64_32
          uint32_t val = static_cast<uint32_t>(S + A);
          module_vmo.write(&val, vmo_patch_offset, sizeof(val));
          break;
        }
        case 11: {  // R_X86_64_32S
          int32_t val = static_cast<int32_t>(S + A);
          module_vmo.write(&val, vmo_patch_offset, sizeof(val));
          break;
        }
        default:
          fprintf(stderr,
                  "driverhub: process_manager: unhandled x86_64 "
                  "reloc type %u\n",
                  rtype);
          break;
      }
#else
      (void)S;
      (void)A;
      (void)P;
      fprintf(stderr,
              "driverhub: process_manager: relocations not supported "
              "on this arch\n");
      break;
#endif
    }
  }

  // Find init_module and cleanup_module entry points.
  for (size_t i = 0; i < symcount; i++) {
    const char* sym_name = strtab + symtab[i].st_name;
    if (strcmp(sym_name, "init_module") == 0 && sym_addrs[i] != 0) {
      init_fn_addr_ = sym_addrs[i];
    } else if (strcmp(sym_name, "cleanup_module") == 0 && sym_addrs[i] != 0) {
      exit_fn_addr_ = sym_addrs[i];
    }
  }

  // Keep the VMO alive so the child's mapping remains valid.
  child_mappings_.push_back({std::move(module_vmo), child_base, total_alloc});
  loaded_module_count_++;

  fprintf(stderr,
          "driverhub: process_manager: loaded module into %s at 0x%lx "
          "(%zu bytes, %zu symbols, init=0x%lx, exit=0x%lx)\n",
          name_.c_str(), child_base, total_alloc, symcount, init_fn_addr_,
          exit_fn_addr_);

  return ZX_OK;
}

// ---------------------------------------------------------------------------
// CallModuleInit — start a thread in the child process at init_fn_addr_
// ---------------------------------------------------------------------------

zx_status_t ManagedProcess::CallModuleInit() {
  if (init_fn_addr_ == 0) {
    fprintf(stderr,
            "driverhub: process_manager: no init_fn for %s\n",
            name_.c_str());
    return ZX_ERR_NOT_FOUND;
  }

  // Create a thread in the child process.
  zx::thread thread;
  const char* thread_name = "module_init";
  zx_status_t status = zx::thread::create(process_, thread_name,
                                           strlen(thread_name), 0, &thread);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: thread create failed: %s\n",
            zx_status_get_string(status));
    return status;
  }

  // Allocate a stack in the child's VMAR for this thread.
  constexpr size_t kStackSize = 256 * 1024;  // 256KB stack.
  zx::vmo stack_vmo;
  status = zx::vmo::create(kStackSize, 0, &stack_vmo);
  if (status != ZX_OK) return status;
  stack_vmo.set_property(ZX_PROP_NAME, "module-stack", 12);

  // Map the stack into the child's address space.
  zx_vaddr_t stack_base = 0;
  status = root_vmar_.map(ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                          stack_vmo, 0, kStackSize, &stack_base);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: stack map failed: %s\n",
            zx_status_get_string(status));
    return status;
  }

  // Stack pointer starts at the top (stack grows downward).
  // Align to 16 bytes per ABI requirement.
  uintptr_t sp = (stack_base + kStackSize) & ~15ULL;

  // Start the thread (or the process if not yet started).
  if (!process_started_) {
    // First thread: use zx_process_start which starts the process.
    // arg1 (handle) = ZX_HANDLE_INVALID, arg2 = 0.
    status = process_.start(thread, init_fn_addr_, sp,
                            zx::handle(), 0);
    if (status != ZX_OK) {
      fprintf(stderr,
              "driverhub: process_manager: process start failed: %s\n",
              zx_status_get_string(status));
      return status;
    }
    process_started_ = true;
  } else {
    // Subsequent modules in a colocated process: start a new thread.
    status = thread.start(init_fn_addr_, sp, 0, 0);
    if (status != ZX_OK) {
      fprintf(stderr,
              "driverhub: process_manager: thread start failed: %s\n",
              zx_status_get_string(status));
      return status;
    }
  }

  // Wait for the init thread to complete. Module init_module() should
  // return promptly. Use a generous timeout to avoid blocking forever
  // on a buggy module.
  zx_signals_t observed = 0;
  status = thread.wait_one(ZX_THREAD_TERMINATED,
                           zx::deadline_after(zx::sec(30)), &observed);
  if (status == ZX_ERR_TIMED_OUT) {
    fprintf(stderr,
            "driverhub: process_manager: init_module timed out for %s\n",
            name_.c_str());
    return ZX_ERR_TIMED_OUT;
  }
  if (status != ZX_OK) return status;

  // Check the thread's state for crash info.
  zx_info_thread_t thread_info;
  status = thread.get_info(ZX_INFO_THREAD, &thread_info, sizeof(thread_info),
                           nullptr, nullptr);
  if (status != ZX_OK) return status;

  fprintf(stderr,
          "driverhub: process_manager: init_module completed for %s "
          "(thread state=%u)\n",
          name_.c_str(), thread_info.state);

  // Keep the stack VMO alive.
  child_mappings_.push_back({std::move(stack_vmo), stack_base, kStackSize});

  return ZX_OK;
}

zx_status_t ManagedProcess::CallModuleExit() {
  if (exit_fn_addr_ == 0) {
    return ZX_OK;  // Module has no cleanup function.
  }

  // Create a thread for cleanup_module, same pattern as CallModuleInit.
  zx::thread thread;
  const char* thread_name = "module_exit";
  zx_status_t status = zx::thread::create(process_, thread_name,
                                           strlen(thread_name), 0, &thread);
  if (status != ZX_OK) return status;

  constexpr size_t kStackSize = 256 * 1024;
  zx::vmo stack_vmo;
  status = zx::vmo::create(kStackSize, 0, &stack_vmo);
  if (status != ZX_OK) return status;

  zx_vaddr_t stack_base = 0;
  status = root_vmar_.map(ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                          stack_vmo, 0, kStackSize, &stack_base);
  if (status != ZX_OK) return status;

  uintptr_t sp = (stack_base + kStackSize) & ~15ULL;

  status = thread.start(exit_fn_addr_, sp, 0, 0);
  if (status != ZX_OK) return status;

  zx_signals_t observed = 0;
  status = thread.wait_one(ZX_THREAD_TERMINATED,
                           zx::deadline_after(zx::sec(30)), &observed);
  if (status == ZX_ERR_TIMED_OUT) {
    fprintf(stderr,
            "driverhub: process_manager: cleanup_module timed out for %s\n",
            name_.c_str());
    return ZX_ERR_TIMED_OUT;
  }

  fprintf(stderr,
          "driverhub: process_manager: cleanup_module completed for %s\n",
          name_.c_str());

  return ZX_OK;
}

// ---------------------------------------------------------------------------
// MonitorForCrash — async wait on ZX_PROCESS_TERMINATED
// ---------------------------------------------------------------------------

void ManagedProcess::MonitorForCrash(async_dispatcher_t* dispatcher,
                                      CrashCallback callback) {
  if (!process_.is_valid()) return;

  crash_callback_ = std::move(callback);

  // Set up an async wait on the process handle for ZX_PROCESS_TERMINATED.
  process_wait_ = std::make_unique<async::Wait>(
      process_.get(), ZX_PROCESS_TERMINATED, 0,
      [this](async_dispatcher_t* dispatcher, async::Wait* wait,
             zx_status_t status, const zx_packet_signal_t* signal) {
        if (status != ZX_OK) {
          fprintf(stderr,
                  "driverhub: process_manager: wait error for %s: %s\n",
                  name_.c_str(), zx_status_get_string(status));
          return;
        }

        // Get the process exit info.
        zx_info_process_t proc_info;
        zx_status_t info_status = process_.get_info(
            ZX_INFO_PROCESS, &proc_info, sizeof(proc_info), nullptr, nullptr);

        zx_status_t exit_code = ZX_ERR_INTERNAL;
        if (info_status == ZX_OK) {
          exit_code = proc_info.return_code;
          fprintf(stderr,
                  "driverhub: process %s terminated (return_code=%ld, "
                  "started=%d)\n",
                  name_.c_str(), proc_info.return_code,
                  proc_info.flags & ZX_INFO_PROCESS_FLAG_STARTED ? 1 : 0);
        }

        if (crash_callback_) {
          crash_callback_(exit_code);
        }
      });

  zx_status_t status = process_wait_->Begin(dispatcher);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: failed to begin wait for %s: %s\n",
            name_.c_str(), zx_status_get_string(status));
    process_wait_.reset();
  }
}

// ---------------------------------------------------------------------------
// ProcessManager
// ---------------------------------------------------------------------------

ProcessManager::ProcessManager(async_dispatcher_t* dispatcher)
    : dispatcher_(dispatcher) {}

ProcessManager::~ProcessManager() = default;

std::unique_ptr<ManagedProcess> ProcessManager::CreateProcess(
    const std::string& name) {
  zx::process child_process;
  zx::vmar child_vmar;

  std::string process_name = "driverhub:" + name;
  if (process_name.size() >= ZX_MAX_NAME_LEN) {
    process_name.resize(ZX_MAX_NAME_LEN - 1);
  }

  zx_status_t status = zx::process::create(
      *zx::job::default_job(), process_name.c_str(), process_name.size(), 0u,
      &child_process, &child_vmar);

  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: process_manager: failed to create process %s: %s\n",
            name.c_str(), zx_status_get_string(status));
    return nullptr;
  }

  fprintf(stderr,
          "driverhub: process_manager: created process %s (koid=%lu)\n",
          process_name.c_str(), child_process.get());

  auto managed = std::make_unique<ManagedProcess>(
      name, std::move(child_process), std::move(child_vmar));

  managed->MonitorForCrash(dispatcher_, [name](zx_status_t exit_code) {
    fprintf(stderr,
            "driverhub: process %s terminated with status %s\n",
            name.c_str(), zx_status_get_string(exit_code));
  });

  return managed;
}

}  // namespace driverhub
