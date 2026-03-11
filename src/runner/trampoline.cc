// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runner/trampoline.h"

#include <zircon/status.h>

#include <cstdio>
#include <cstring>

namespace driverhub {

// ============================================================
// AArch64 Trampoline
// ============================================================
//
// The AArch64 C calling convention passes arguments in x0-x7 and returns
// in x0. The trampoline:
//
//   1. Save x0-x7 (up to num_args) into the shared VMO buffer.
//   2. Save LR (x30) on the stack.
//   3. Set x0 = shared_vmo_addr (arg to dispatch function).
//   4. BLR to dispatch_addr.
//   5. Load return value from VMO result area into x0.
//   6. Restore LR and return.
//
// VMO layout: [8B: num_args] [8B * 8: arg values] [8B: result]
//
// The dispatch function signature is:
//   uint64_t dispatch(uint64_t* vmo_buf);
// It reads args from vmo_buf, makes the FIDL call, writes result, returns.

std::vector<uint8_t> GenerateTrampolineAArch64(uint64_t shared_vmo_addr,
                                                uint64_t dispatch_addr,
                                                size_t num_args) {
  if (num_args > kMaxTrampolineArgs) num_args = kMaxTrampolineArgs;

  std::vector<uint32_t> insns;

  // Helper to emit MOVZ/MOVK to load a 64-bit immediate into a register.
  auto emit_mov_imm64 = [&insns](uint32_t rd, uint64_t imm) {
    // MOVZ Xrd, #imm16, LSL #0
    insns.push_back(0xD2800000 | (rd & 0x1F) |
                    (static_cast<uint32_t>(imm & 0xFFFF) << 5));
    // MOVK Xrd, #imm16, LSL #16
    if (imm > 0xFFFF) {
      insns.push_back(0xF2A00000 | (rd & 0x1F) |
                      (static_cast<uint32_t>((imm >> 16) & 0xFFFF) << 5));
    }
    // MOVK Xrd, #imm16, LSL #32
    if (imm > 0xFFFFFFFF) {
      insns.push_back(0xF2C00000 | (rd & 0x1F) |
                      (static_cast<uint32_t>((imm >> 32) & 0xFFFF) << 5));
    }
    // MOVK Xrd, #imm16, LSL #48
    if (imm > 0xFFFFFFFFFFFF) {
      insns.push_back(0xF2E00000 | (rd & 0x1F) |
                      (static_cast<uint32_t>((imm >> 48) & 0xFFFF) << 5));
    }
  };

  // STP x29, x30, [SP, #-16]!  — save frame pointer and link register
  insns.push_back(0xA9BF7BFD);

  // MOV x29, SP
  insns.push_back(0x910003FD);

  // Load shared_vmo_addr into x9 (scratch register).
  emit_mov_imm64(9, shared_vmo_addr);

  // Store num_args at [x9] (first 8 bytes of VMO buffer).
  emit_mov_imm64(10, num_args);
  // STR x10, [x9]
  insns.push_back(0xF900012A);

  // Store each argument register x0-x(num_args-1) at [x9 + 8 + i*8].
  for (size_t i = 0; i < num_args; i++) {
    // STR x<i>, [x9, #(8 + i*8)]
    uint32_t offset = static_cast<uint32_t>((8 + i * 8) / 8);
    insns.push_back(0xF9000120 | (offset << 10) | (i & 0x1F));
  }

  // Set x0 = shared_vmo_addr (first arg to dispatch function).
  // MOV x0, x9
  insns.push_back(0xAA0903E0);

  // Load dispatch_addr into x10 and call it.
  emit_mov_imm64(10, dispatch_addr);
  // BLR x10
  insns.push_back(0xD63F0140);

  // The dispatch function returns the result in x0. We're done.
  // (If the dispatch writes the result into the VMO too, callers can
  // read it from there for struct returns.)

  // LDP x29, x30, [SP], #16  — restore frame pointer and link register
  insns.push_back(0xA8C17BFD);

  // RET
  insns.push_back(0xD65F03C0);

  // Convert to bytes.
  std::vector<uint8_t> code(insns.size() * 4);
  memcpy(code.data(), insns.data(), code.size());
  return code;
}

// ============================================================
// x86_64 Trampoline
// ============================================================
//
// The x86_64 System V calling convention passes arguments in
// rdi, rsi, rdx, rcx, r8, r9 and returns in rax.
//
// The trampoline:
//   1. Save callee-saved registers (rbx, rbp).
//   2. Store argument registers into the VMO buffer.
//   3. Set rdi = shared_vmo_addr.
//   4. Call dispatch_addr.
//   5. Return value is already in rax.
//   6. Restore callee-saved registers and return.

std::vector<uint8_t> GenerateTrampolineX86_64(uint64_t shared_vmo_addr,
                                               uint64_t dispatch_addr,
                                               size_t num_args) {
  if (num_args > 6) num_args = 6;  // x86_64 has 6 register args.

  std::vector<uint8_t> code;

  auto emit = [&code](std::initializer_list<uint8_t> bytes) {
    for (auto b : bytes) code.push_back(b);
  };

  auto emit_u64 = [&code](uint64_t val) {
    for (int i = 0; i < 8; i++) {
      code.push_back(static_cast<uint8_t>(val >> (i * 8)));
    }
  };

  auto emit_u32 = [&code](uint32_t val) {
    for (int i = 0; i < 4; i++) {
      code.push_back(static_cast<uint8_t>(val >> (i * 8)));
    }
  };

  // push rbp
  emit({0x55});
  // mov rbp, rsp
  emit({0x48, 0x89, 0xE5});
  // push rbx
  emit({0x53});

  // mov rbx, shared_vmo_addr (movabs rbx, imm64)
  emit({0x48, 0xBB});
  emit_u64(shared_vmo_addr);

  // Store num_args at [rbx]
  // mov qword [rbx], num_args
  emit({0x48, 0xC7, 0x03});
  emit_u32(static_cast<uint32_t>(num_args));

  // Store argument registers into [rbx + 8 + i*8].
  // Register order: rdi, rsi, rdx, rcx, r8, r9
  // mov [rbx + offset], reg
  static const uint8_t arg_reg_encodings[][3] = {
      {0x48, 0x89, 0x7B},  // rdi -> [rbx + disp8]
      {0x48, 0x89, 0x73},  // rsi -> [rbx + disp8]
      {0x48, 0x89, 0x53},  // rdx -> [rbx + disp8]
      {0x48, 0x89, 0x4B},  // rcx -> [rbx + disp8]
      {0x4C, 0x89, 0x43},  // r8  -> [rbx + disp8]
      {0x4C, 0x89, 0x4B},  // r9  -> [rbx + disp8]
  };

  for (size_t i = 0; i < num_args; i++) {
    emit({arg_reg_encodings[i][0], arg_reg_encodings[i][1],
          arg_reg_encodings[i][2]});
    code.push_back(static_cast<uint8_t>(8 + i * 8));
  }

  // Set rdi = shared_vmo_addr (first arg to dispatch function).
  // mov rdi, rbx
  emit({0x48, 0x89, 0xDF});

  // mov rax, dispatch_addr (movabs rax, imm64)
  emit({0x48, 0xB8});
  emit_u64(dispatch_addr);

  // call rax
  emit({0xFF, 0xD0});

  // Return value is already in rax.

  // pop rbx
  emit({0x5B});
  // pop rbp
  emit({0x5D});
  // ret
  emit({0xC3});

  return code;
}

// ============================================================
// Architecture dispatch
// ============================================================

std::vector<uint8_t> GenerateTrampoline(uint64_t shared_vmo_addr,
                                         uint64_t dispatch_addr,
                                         size_t num_args) {
#if defined(__aarch64__)
  return GenerateTrampolineAArch64(shared_vmo_addr, dispatch_addr, num_args);
#elif defined(__x86_64__)
  return GenerateTrampolineX86_64(shared_vmo_addr, dispatch_addr, num_args);
#else
#error "Unsupported architecture for trampoline generation"
#endif
}

zx_status_t WriteTrampolineToVmo(const zx::vmo& vmo,
                                  uint64_t offset,
                                  const std::vector<uint8_t>& code) {
  zx_status_t status = vmo.write(code.data(), offset, code.size());
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: trampoline: failed to write %zu bytes at offset "
            "0x%lx: %s\n",
            code.size(), offset, zx_status_get_string(status));
  }
  return status;
}

}  // namespace driverhub
