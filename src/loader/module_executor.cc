// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/loader/module_executor.h"

#include <cstdio>
#include <cstdint>

#if defined(__Fuchsia__)
// Fuchsia: use Zircon exception channels.
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/debug.h>
#include <zircon/syscalls/exception.h>
#include <zircon/threads.h>
#include <threads.h>
#include <atomic>
#else
// Host (Linux/macOS): use POSIX signals with sigsetjmp/siglongjmp.
#include <setjmp.h>
#include <signal.h>
#include <cstring>
#endif

namespace driverhub {

#if defined(__Fuchsia__)

// ── Fuchsia implementation ──────────────────────────────────────────
//
// Strategy: install a process-wide exception channel, call the module
// function, and if a fault exception arrives, mark it and resume the
// faulting thread by advancing PC past the faulting instruction.
//
// For init_module, the simplest recovery is to:
//   1. Detect the fault
//   2. Log the faulting address
//   3. Set the thread's return value register (x0) to an error code
//   4. Set PC to the function's return path (LR)
//
// This allows the caller to see MODULE_EXEC_FAULT and continue.

namespace {

struct ExecContext {
  std::atomic<bool> faulted{false};
  std::atomic<uint64_t> fault_addr{0};
};

// Exception watcher thread function.
// Loops to handle multiple exceptions (e.g. many privileged MRS/MSR
// instructions that need emulation) until the channel is closed.
int ExceptionWatcherThread(void* arg) {
  zx_handle_t channel = static_cast<zx_handle_t>(reinterpret_cast<uintptr_t>(arg));

  for (;;) {
    // Wait for an exception or channel close.
    zx_wait_item_t items[1] = {{channel, ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED, 0}};
    zx_status_t status = zx_object_wait_many(items, 1, ZX_TIME_INFINITE);
    if (status != ZX_OK || !(items[0].pending & ZX_CHANNEL_READABLE)) {
      break;  // Channel closed (module finished or no more exceptions).
    }

    // Read the exception.
    zx_exception_info_t info;
    zx_handle_t exception;
    uint32_t actual_bytes, actual_handles;
    status = zx_channel_read(channel, 0, &info, &exception,
                              sizeof(info), 1, &actual_bytes, &actual_handles);
    if (status != ZX_OK) break;

    // Get the faulting thread and its registers.
    zx_handle_t thread;
    status = zx_exception_get_thread(exception, &thread);
    if (status == ZX_OK) {
      zx_thread_state_general_regs_t regs;
      status = zx_thread_read_state(thread, ZX_THREAD_STATE_GENERAL_REGS,
                                     &regs, sizeof(regs));
      if (status == ZX_OK) {
        // Try to emulate privileged ARM64 instructions before giving up.
        bool emulated = false;
#if defined(__aarch64__)
        uint32_t insn = 0;
        memcpy(&insn, reinterpret_cast<const void*>(regs.pc), 4);

        bool is_msr = (insn & 0xFFF00000u) == 0xD5100000u;
        bool is_mrs = (insn & 0xFFF00000u) == 0xD5300000u;
        bool is_daif_imm = (insn & 0xFFFFF0DFu) == 0xD50340DFu;

        if (is_msr || is_mrs || is_daif_imm) {
          uint32_t rt = insn & 0x1F;
          if (is_mrs && rt < 30) {
            regs.r[rt] = 0;
          }
          regs.pc += 4;
          zx_thread_write_state(thread, ZX_THREAD_STATE_GENERAL_REGS,
                                 &regs, sizeof(regs));
          emulated = true;
        }
#endif

        if (!emulated) {
          fprintf(stderr,
                  "[module_executor] FAULT in module code!\n"
                  "  PC:  0x%016lx\n"
                  "  LR:  0x%016lx\n"
                  "  SP:  0x%016lx\n",
                  (unsigned long)regs.pc,
                  (unsigned long)regs.lr,
                  (unsigned long)regs.sp);

          // Recover: set x0 = MODULE_EXEC_FAULT, PC = LR.
          regs.r[0] = static_cast<uint64_t>(static_cast<int64_t>(MODULE_EXEC_FAULT));
          regs.pc = regs.lr;
          zx_thread_write_state(thread, ZX_THREAD_STATE_GENERAL_REGS,
                                 &regs, sizeof(regs));
        }
      }
      zx_handle_close(thread);
    }

    // Mark exception as handled and resume.
    uint32_t state = ZX_EXCEPTION_STATE_HANDLED;
    zx_object_set_property(exception, ZX_PROP_EXCEPTION_STATE, &state, sizeof(state));
    zx_handle_close(exception);
  }

  return 0;
}

int SafeCallWithExceptionChannel(void* fn, bool is_init) {
  // Create an exception channel for this process.
  zx_handle_t channel;
  zx_status_t status = zx_task_create_exception_channel(
      zx_process_self(), 0, &channel);
  if (status != ZX_OK) {
    fprintf(stderr, "[module_executor] WARNING: cannot create exception "
            "channel (status=%d), calling without protection\n", status);
    if (is_init) {
      return reinterpret_cast<int (*)()>(fn)();
    } else {
      reinterpret_cast<void (*)()>(fn)();
      return 0;
    }
  }

  // Start exception watcher thread.
  thrd_t watcher;
  thrd_create(&watcher, ExceptionWatcherThread,
              reinterpret_cast<void*>(static_cast<uintptr_t>(channel)));

  // Call the module function.
  int result;
  if (is_init) {
    result = reinterpret_cast<int (*)()>(fn)();
  } else {
    reinterpret_cast<void (*)()>(fn)();
    result = 0;
  }

  // Close the channel to unblock the watcher thread, then join.
  zx_handle_close(channel);
  thrd_join(watcher, nullptr);

  return result;
}

}  // namespace

int SafeCallInit(int (*init_fn)()) {
  if (!init_fn) return -1;
  fprintf(stderr, "[module_executor] calling init_module at %p (Fuchsia)\n",
          reinterpret_cast<void*>(init_fn));
  return SafeCallWithExceptionChannel(reinterpret_cast<void*>(init_fn), true);
}

int SafeCallExit(void (*exit_fn)()) {
  if (!exit_fn) return -1;
  fprintf(stderr, "[module_executor] calling cleanup_module at %p (Fuchsia)\n",
          reinterpret_cast<void*>(exit_fn));
  return SafeCallWithExceptionChannel(reinterpret_cast<void*>(exit_fn), false);
}

#else  // Host (Linux/macOS)

// ── POSIX implementation ────────────────────────────────────────────
//
// Uses sigsetjmp/siglongjmp with SIGSEGV/SIGBUS/SIGILL handlers.

namespace {

// Thread-local jump buffer for fault recovery.
static thread_local sigjmp_buf g_jmp_buf;
static thread_local volatile bool g_handler_active = false;

void FaultSignalHandler(int sig, siginfo_t* info, void* ucontext) {
  if (!g_handler_active) {
    // Not our call — re-raise.
    signal(sig, SIG_DFL);
    raise(sig);
    return;
  }

#if defined(__aarch64__) && !defined(__APPLE__)
  // Try to emulate privileged ARM64 instructions (MRS/MSR to EL1 regs).
  // This handles any instructions that weren't caught by static patching.
  if (sig == SIGILL) {
    auto* uc = static_cast<ucontext_t*>(ucontext);
    uint32_t insn = 0;
    memcpy(&insn, reinterpret_cast<const void*>(uc->uc_mcontext.pc), 4);

    bool is_msr = (insn & 0xFFF00000u) == 0xD5100000u;
    bool is_mrs = (insn & 0xFFF00000u) == 0xD5300000u;
    bool is_daif_imm = (insn & 0xFFFFF0DFu) == 0xD50340DFu;

    if (is_msr || is_mrs || is_daif_imm) {
      uint32_t rt = insn & 0x1F;
      if (is_mrs && rt < 30) {
        uc->uc_mcontext.regs[rt] = 0;
      }
      uc->uc_mcontext.pc += 4;
      return;  // Resume execution at next instruction.
    }
  }
#endif

  const char* sig_name = "UNKNOWN";
  if (sig == SIGSEGV) sig_name = "SIGSEGV";
  else if (sig == SIGBUS) sig_name = "SIGBUS";
  else if (sig == SIGILL) sig_name = "SIGILL";

  fprintf(stderr,
          "[module_executor] FAULT in module code!\n"
          "  Signal: %s (%d)\n"
          "  Fault addr: %p\n",
          sig_name, sig,
          info ? info->si_addr : nullptr);

#if defined(__aarch64__) && !defined(__APPLE__)
  auto* uc = static_cast<ucontext_t*>(ucontext);
  fprintf(stderr, "  PC: 0x%016lx\n  LR: 0x%016lx\n",
          (unsigned long)uc->uc_mcontext.pc,
          (unsigned long)uc->uc_mcontext.regs[30]);
#elif defined(__x86_64__) && !defined(__APPLE__)
  auto* uc = static_cast<ucontext_t*>(ucontext);
  fprintf(stderr, "  RIP: 0x%016llx\n",
          (unsigned long long)uc->uc_mcontext.gregs[REG_RIP]);
#else
  (void)ucontext;
#endif

  siglongjmp(g_jmp_buf, 1);
}

void InstallFaultHandlers(struct sigaction* old_segv,
                          struct sigaction* old_bus,
                          struct sigaction* old_ill) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = FaultSignalHandler;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGSEGV, &sa, old_segv);
  sigaction(SIGBUS, &sa, old_bus);
  sigaction(SIGILL, &sa, old_ill);
}

void RestoreFaultHandlers(const struct sigaction* old_segv,
                          const struct sigaction* old_bus,
                          const struct sigaction* old_ill) {
  sigaction(SIGSEGV, old_segv, nullptr);
  sigaction(SIGBUS, old_bus, nullptr);
  sigaction(SIGILL, old_ill, nullptr);
}

}  // namespace

int SafeCallInit(int (*init_fn)()) {
  if (!init_fn) return -1;
  fprintf(stderr, "[module_executor] calling init_module at %p (host)\n",
          reinterpret_cast<void*>(init_fn));

  struct sigaction old_segv, old_bus, old_ill;
  InstallFaultHandlers(&old_segv, &old_bus, &old_ill);

  int result;
  g_handler_active = true;
  if (sigsetjmp(g_jmp_buf, 1) == 0) {
    result = init_fn();
  } else {
    result = MODULE_EXEC_FAULT;
  }
  g_handler_active = false;

  RestoreFaultHandlers(&old_segv, &old_bus, &old_ill);
  return result;
}

int SafeCallExit(void (*exit_fn)()) {
  if (!exit_fn) return -1;
  fprintf(stderr, "[module_executor] calling cleanup_module at %p (host)\n",
          reinterpret_cast<void*>(exit_fn));

  struct sigaction old_segv, old_bus, old_ill;
  InstallFaultHandlers(&old_segv, &old_bus, &old_ill);

  int result;
  g_handler_active = true;
  if (sigsetjmp(g_jmp_buf, 1) == 0) {
    exit_fn();
    result = 0;
  } else {
    result = MODULE_EXEC_FAULT;
  }
  g_handler_active = false;

  RestoreFaultHandlers(&old_segv, &old_bus, &old_ill);
  return result;
}

#endif  // __Fuchsia__

}  // namespace driverhub
