// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/fs_events.h"

#include <cstdio>

namespace driverhub {

namespace {
std::mutex g_callback_mu;
FsEventCallback g_callback;
}  // namespace

void FsEventsSetCallback(FsEventCallback callback) {
  std::lock_guard<std::mutex> lock(g_callback_mu);
  g_callback = std::move(callback);
  fprintf(stderr, "driverhub: fs_events: callback registered\n");
}

void FsEventsClearCallback() {
  std::lock_guard<std::mutex> lock(g_callback_mu);
  g_callback = nullptr;
  fprintf(stderr, "driverhub: fs_events: callback cleared\n");
}

void FsEventNotify(FsEventInfo event) {
  std::lock_guard<std::mutex> lock(g_callback_mu);
  if (g_callback) {
    g_callback(event);
  }
}

}  // namespace driverhub
