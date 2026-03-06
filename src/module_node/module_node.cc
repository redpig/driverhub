// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/module_node/module_node.h"

#include <cstdio>

namespace driverhub {

ModuleNode::ModuleNode(std::unique_ptr<LoadedModule> module)
    : module_(std::move(module)) {}

ModuleNode::~ModuleNode() {
  if (bound_) {
    Unbind();
  }
  // The module's MemoryAllocation is automatically released via its
  // unique_ptr destructor when the LoadedModule is destroyed.
}

zx_status_t ModuleNode::Bind() {
  if (bound_) {
    fprintf(stderr, "driverhub: %s: already bound\n", module_->name.c_str());
    return -1;  // ZX_ERR_ALREADY_BOUND
  }

  if (!module_->init_fn) {
    fprintf(stderr, "driverhub: %s: no init_module entry point\n",
            module_->name.c_str());
    return -1;  // ZX_ERR_NOT_FOUND
  }

  fprintf(stderr, "driverhub: calling init_module for %s\n",
          module_->name.c_str());

  int ret = module_->init_fn();
  if (ret != 0) {
    fprintf(stderr, "driverhub: %s: init_module returned %d\n",
            module_->name.c_str(), ret);
    return -1;  // ZX_ERR_INTERNAL
  }

  fprintf(stderr, "driverhub: %s: init_module succeeded\n",
          module_->name.c_str());
  bound_ = true;
  return 0;  // ZX_OK
}

void ModuleNode::Unbind() {
  if (!bound_) return;

  if (module_->exit_fn) {
    fprintf(stderr, "driverhub: calling cleanup_module for %s\n",
            module_->name.c_str());
    module_->exit_fn();
  }

  bound_ = false;
  fprintf(stderr, "driverhub: %s: unbound\n", module_->name.c_str());
}

}  // namespace driverhub
