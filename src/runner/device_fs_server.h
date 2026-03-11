// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_RUNNER_DEVICE_FS_SERVER_H_
#define DRIVERHUB_SRC_RUNNER_DEVICE_FS_SERVER_H_

#include <fidl/fuchsia.driverhub/cpp/fidl.h>
#include <lib/async/dispatcher.h>

#include <mutex>
#include <string>
#include <vector>

namespace driverhub {

// Forward declaration.
struct VfsNode;

// FIDL server implementing fuchsia.driverhub.DeviceFs.
//
// Aggregates state from the three VFS singletons (DevFs, SysFs, ProcFs) and
// serves it to Starnix via the DeviceFs protocol. Supports watching for
// filesystem entry changes via DeviceFsWatcher event streams.
//
// Thread-safe: shim code may call NotifyEvent() from any thread while FIDL
// methods are being dispatched on the async loop.
class DeviceFsServer : public fidl::Server<fuchsia_driverhub::DeviceFs> {
 public:
  explicit DeviceFsServer(async_dispatcher_t* dispatcher);
  ~DeviceFsServer() override;

  // fuchsia.driverhub.DeviceFs implementation.
  void ListEntries(ListEntriesCompleter::Sync& completer) override;
  void Watch(WatchRequest& request, WatchCompleter::Sync& completer) override;
  void ReadAttribute(ReadAttributeRequest& request,
                     ReadAttributeCompleter::Sync& completer) override;
  void WriteAttribute(WriteAttributeRequest& request,
                      WriteAttributeCompleter::Sync& completer) override;
  void OpenDevice(OpenDeviceRequest& request,
                  OpenDeviceCompleter::Sync& completer) override;

  // Called by shim code when VFS entries are added, removed, or modified.
  // Broadcasts an FsEvent to all active watchers.
  void NotifyEvent(fuchsia_driverhub::FsEvent event);

 private:
  // Convert a VfsNode to an FsEntry FIDL table.
  static fuchsia_driverhub::FsEntry VfsNodeToFsEntry(const VfsNode* node);

  // Dispatch path to the correct VFS service based on prefix.
  // Returns true and sets |prefix| to "dev", "sys", or "proc" on match.
  static bool ClassifyPath(const std::string& path, std::string& prefix);

  async_dispatcher_t* dispatcher_;

  mutable std::mutex watchers_mu_;
  std::vector<fidl::ServerBinding<fuchsia_driverhub::DeviceFsWatcher>>
      watchers_ __attribute__((guarded_by(watchers_mu_)));
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_RUNNER_DEVICE_FS_SERVER_H_
