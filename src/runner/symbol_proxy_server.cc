// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runner/symbol_proxy_server.h"

#include <lib/zx/thread.h>
#include <lib/zx/vmo.h>
#include <zircon/status.h>

#include <cstdio>
#include <cstring>

namespace driverhub {

SymbolProxyServer::SymbolProxyServer(uint64_t target_addr,
                                     const zx::process& target_process,
                                     const zx::vmar& target_vmar,
                                     std::string symbol_name)
    : target_addr_(target_addr),
      target_process_(target_process),
      target_vmar_(target_vmar),
      symbol_name_(std::move(symbol_name)) {}

SymbolProxyServer::~SymbolProxyServer() = default;

void SymbolProxyServer::Call(CallRequest& request,
                             CallCompleter::Sync& completer) {
  const auto& args = request.args();

  fprintf(stderr,
          "driverhub: symbol_proxy: Call(%s) addr=0x%lx args_size=%zu\n",
          symbol_name_.c_str(), target_addr_, args.size());

  // The proxy call mechanism:
  //
  // 1. Write the serialized arguments into a VMO mapped in the target process.
  // 2. Create a thread in the target process that:
  //    a. Reads the arguments from the shared VMO.
  //    b. Calls the target function with the deserialized arguments.
  //    c. Writes the return value back to the VMO.
  // 3. Wait for the thread to complete.
  // 4. Read the return value from the VMO and send it back.
  //
  // For the initial implementation, we use a simple protocol:
  // - VMO layout: [8 bytes: arg_size] [args...] [8 bytes: result_size] [result...]
  // - The trampoline code in the target process reads args, calls the function,
  //   and writes the result.

  constexpr size_t kProxyVmoSize = 64 * 1024;  // 64KB for args + result.

  if (args.size() + 16 > kProxyVmoSize) {
    completer.Reply(fit::error(ZX_ERR_BUFFER_TOO_SMALL));
    return;
  }

  // Create a VMO shared between the runner and target process for argument
  // and result passing.
  zx::vmo proxy_vmo;
  zx_status_t status = zx::vmo::create(kProxyVmoSize, 0, &proxy_vmo);
  if (status != ZX_OK) {
    completer.Reply(fit::error(status));
    return;
  }

  // Write argument size and data into the VMO.
  uint64_t arg_size = args.size();
  status = proxy_vmo.write(&arg_size, 0, sizeof(arg_size));
  if (status != ZX_OK) {
    completer.Reply(fit::error(status));
    return;
  }
  if (arg_size > 0) {
    status = proxy_vmo.write(args.data(), sizeof(arg_size), arg_size);
    if (status != ZX_OK) {
      completer.Reply(fit::error(status));
      return;
    }
  }

  // Map the VMO into the target process's address space.
  zx_vaddr_t target_vmo_addr = 0;
  status = target_vmar_.map(ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                            proxy_vmo, 0, kProxyVmoSize, &target_vmo_addr);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: symbol_proxy: failed to map proxy VMO into "
            "target: %s\n",
            zx_status_get_string(status));
    completer.Reply(fit::error(status));
    return;
  }

  // Create a thread in the target process to execute the proxied function.
  //
  // The trampoline needs to:
  // 1. Read args from target_vmo_addr
  // 2. Call the function at target_addr_ with appropriate calling convention
  // 3. Write result back to target_vmo_addr + offset
  // 4. Exit the thread
  //
  // For simple functions with <= 8 register args (common in the Linux kernel
  // ABI), the calling convention passes args in x0-x7 on AArch64 or
  // rdi/rsi/rdx/rcx/r8/r9 on x86_64. The proxy deserializes from the VMO
  // into registers before the call.
  //
  // For the initial implementation, we support functions with a single
  // pointer argument (the VMO address containing serialized args) and
  // use a small trampoline stub.
  //
  // TODO(driverhub): Generate architecture-specific trampoline stubs that
  // properly marshal arguments according to the function's C signature.
  // This will require a function signature registry alongside the symbol
  // registry.

  // For now, call the function directly with the VMO address as arg1.
  // This works for functions that accept a single pointer/context argument.
  zx::thread proxy_thread;
  status = zx::thread::create(target_process_, "sym_proxy",
                               strlen("sym_proxy"), 0, &proxy_thread);
  if (status != ZX_OK) {
    completer.Reply(fit::error(status));
    return;
  }

  // Allocate a small stack for the proxy thread.
  constexpr size_t kStackSize = 64 * 1024;
  zx::vmo stack_vmo;
  status = zx::vmo::create(kStackSize, 0, &stack_vmo);
  if (status != ZX_OK) {
    completer.Reply(fit::error(status));
    return;
  }

  zx_vaddr_t stack_base = 0;
  status = target_vmar_.map(ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                            stack_vmo, 0, kStackSize, &stack_base);
  if (status != ZX_OK) {
    completer.Reply(fit::error(status));
    return;
  }

  uintptr_t sp = (stack_base + kStackSize) & ~15ULL;

  // Start the thread at the target function with the VMO address as arg1.
  // On AArch64: x0 = arg1 (target_vmo_addr), entry = target_addr_.
  // On x86_64: rdi = arg1 (target_vmo_addr), entry = target_addr_.
  status = proxy_thread.start(target_addr_, sp, target_vmo_addr, 0);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: symbol_proxy: thread start failed: %s\n",
            zx_status_get_string(status));
    completer.Reply(fit::error(status));
    return;
  }

  // Wait for the proxy thread to complete.
  zx_signals_t observed = 0;
  status = proxy_thread.wait_one(ZX_THREAD_TERMINATED,
                                  zx::deadline_after(zx::sec(30)), &observed);
  if (status != ZX_OK) {
    fprintf(stderr,
            "driverhub: symbol_proxy: thread wait failed for %s: %s\n",
            symbol_name_.c_str(), zx_status_get_string(status));
    completer.Reply(fit::error(status));
    return;
  }

  // Read the result from the proxy VMO.
  // Result is placed after the arguments: offset = 8 + arg_size, aligned to 8.
  uint64_t result_offset = AlignUp(sizeof(arg_size) + arg_size, 8);
  uint64_t result_size = 0;
  status = proxy_vmo.read(&result_size, result_offset, sizeof(result_size));
  if (status != ZX_OK || result_size > kProxyVmoSize) {
    // Function may not write a result (void return). Return empty.
    completer.Reply(fit::ok(std::vector<uint8_t>()));
    return;
  }

  std::vector<uint8_t> result(result_size);
  if (result_size > 0) {
    status = proxy_vmo.read(result.data(), result_offset + sizeof(result_size),
                            result_size);
    if (status != ZX_OK) {
      completer.Reply(fit::error(status));
      return;
    }
  }

  completer.Reply(fit::ok(std::move(result)));
}

namespace {

uint64_t AlignUp(uint64_t value, uint64_t align) {
  return (value + align - 1) & ~(align - 1);
}

}  // namespace

fidl::ClientEnd<fuchsia_driverhub::SymbolProxy> CreateSymbolProxy(
    async_dispatcher_t* dispatcher,
    uint64_t target_addr,
    const zx::process& target_process,
    const zx::vmar& target_vmar,
    const std::string& symbol_name) {
  auto endpoints = fidl::CreateEndpoints<fuchsia_driverhub::SymbolProxy>();
  if (!endpoints.is_ok()) {
    fprintf(stderr,
            "driverhub: symbol_proxy: failed to create endpoints for %s\n",
            symbol_name.c_str());
    return {};
  }

  auto server = std::make_unique<SymbolProxyServer>(
      target_addr, target_process, target_vmar, symbol_name);

  fidl::BindServer(dispatcher, std::move(endpoints->server),
                   std::move(server));

  fprintf(stderr,
          "driverhub: symbol_proxy: created proxy for %s (addr=0x%lx)\n",
          symbol_name.c_str(), target_addr);

  return std::move(endpoints->client);
}

}  // namespace driverhub
