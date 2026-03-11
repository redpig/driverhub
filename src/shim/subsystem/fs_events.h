// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_FS_EVENTS_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_FS_EVENTS_H_

// Filesystem event notification for the DeviceFs watcher bridge.
//
// When .ko modules call VFS APIs (device_create, sysfs_create_group, etc.),
// the shim functions call FsEventNotify() to broadcast events to any
// registered watchers (e.g., the DeviceFsServer serving Starnix).
//
// This is a simple callback mechanism that decouples the shim layer from
// FIDL: the runner registers a callback at startup, and the shim just
// calls it when something changes.

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace driverhub {

enum class FsEventType : uint8_t {
  kAdded = 1,
  kRemoved = 2,
  kModified = 3,
};

enum class FsEntryType : uint8_t {
  kDirectory = 1,
  kRegular = 2,
  kCharDevice = 4,
};

// Lightweight event struct passed from shim to runner.
struct FsEventInfo {
  FsEventType type;
  FsEntryType entry_type;
  std::string path;       // Full Linux-style path (e.g., "/dev/rfkill").
  uint32_t mode = 0;
  uint32_t major = 0;
  uint32_t minor = 0;
  std::string module_name;
  // uevent env vars in "KEY=VALUE" format.
  std::vector<std::string> uevent_env;
};

// Callback type for event notifications.
using FsEventCallback = std::function<void(const FsEventInfo&)>;

// Register a global callback to receive VFS events.
// Only one callback can be active at a time (the runner's DeviceFsServer).
void FsEventsSetCallback(FsEventCallback callback);

// Clear the registered callback.
void FsEventsClearCallback();

// Emit an event to the registered callback (if any). Called by shim functions.
void FsEventNotify(FsEventInfo event);

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_FS_EVENTS_H_
