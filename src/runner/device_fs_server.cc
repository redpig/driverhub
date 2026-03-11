// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runner/device_fs_server.h"

#include <zircon/status.h>

#include <cstdio>
#include <cstring>

#include "src/shim/subsystem/fs_events.h"
#include "src/shim/subsystem/vfs_service.h"

namespace driverhub {

namespace {

// Maximum buffer size for sysfs/procfs attribute reads and writes.
constexpr size_t kAttrBufSize = 4096;

}  // namespace

DeviceFsServer::DeviceFsServer(async_dispatcher_t* dispatcher)
    : dispatcher_(dispatcher) {
  // Register the callback so shim code's VFS events are forwarded to
  // our watchers (and ultimately to Starnix).
  FsEventsSetCallback([this](const FsEventInfo& info) {
    fuchsia_driverhub::FsEvent event;

    switch (info.type) {
      case FsEventType::kAdded:
        event.kind() = fuchsia_driverhub::FsEventKind::kAdded;
        break;
      case FsEventType::kRemoved:
        event.kind() = fuchsia_driverhub::FsEventKind::kRemoved;
        break;
      case FsEventType::kModified:
        event.kind() = fuchsia_driverhub::FsEventKind::kModified;
        break;
    }

    fuchsia_driverhub::FsEntry entry;
    entry.path() = info.path;
    entry.mode() = info.mode;
    entry.major() = info.major;
    entry.minor() = info.minor;

    switch (info.entry_type) {
      case FsEntryType::kDirectory:
        entry.kind() = fuchsia_driverhub::FsEntryKind::kDirectory;
        break;
      case FsEntryType::kRegular:
        entry.kind() = fuchsia_driverhub::FsEntryKind::kRegular;
        break;
      case FsEntryType::kCharDevice:
        entry.kind() = fuchsia_driverhub::FsEntryKind::kCharDevice;
        break;
    }

    if (!info.module_name.empty()) {
      entry.module_name() = info.module_name;
    }

    event.entry() = std::move(entry);

    if (!info.uevent_env.empty()) {
      event.uevent_env() = info.uevent_env;
    }

    NotifyEvent(std::move(event));
  });
}

DeviceFsServer::~DeviceFsServer() {
  FsEventsClearCallback();
}

// static
fuchsia_driverhub::FsEntry DeviceFsServer::VfsNodeToFsEntry(
    const VfsNode* node) {
  fuchsia_driverhub::FsEntry entry;
  entry.path() = node->path;
  entry.mode() = static_cast<uint32_t>(node->mode);
  entry.major() = node->major;
  entry.minor() = node->minor;

  switch (node->type) {
    case VfsNodeType::kDirectory:
      entry.kind() = fuchsia_driverhub::FsEntryKind::kDirectory;
      break;
    case VfsNodeType::kCharDevice:
    case VfsNodeType::kMiscDevice:
      entry.kind() = fuchsia_driverhub::FsEntryKind::kCharDevice;
      break;
    case VfsNodeType::kProcEntry:
    case VfsNodeType::kSysfsAttr:
    case VfsNodeType::kDebugfsEntry:
      entry.kind() = fuchsia_driverhub::FsEntryKind::kRegular;
      break;
  }

  return entry;
}

// static
bool DeviceFsServer::ClassifyPath(const std::string& path,
                                   std::string& prefix) {
  if (path.rfind("/dev", 0) == 0 || path.rfind("dev", 0) == 0) {
    prefix = "dev";
    return true;
  }
  if (path.rfind("/sys", 0) == 0 || path.rfind("sys", 0) == 0) {
    prefix = "sys";
    return true;
  }
  if (path.rfind("/proc", 0) == 0 || path.rfind("proc", 0) == 0) {
    prefix = "proc";
    return true;
  }
  return false;
}

void DeviceFsServer::ListEntries(ListEntriesCompleter::Sync& completer) {
  std::vector<fuchsia_driverhub::FsEntry> entries;

  // Collect from DevFs.
  for (const VfsNode* node : DevFs::Instance().ListDevices()) {
    entries.push_back(VfsNodeToFsEntry(node));
  }

  // Collect from SysFs.
  for (const VfsNode* node : SysFs::Instance().ListAll()) {
    entries.push_back(VfsNodeToFsEntry(node));
  }

  // Collect from ProcFs.
  for (const VfsNode* node : ProcFs::Instance().ListAll()) {
    entries.push_back(VfsNodeToFsEntry(node));
  }

  fprintf(stderr, "driverhub: device_fs: ListEntries returning %zu entries\n",
          entries.size());

  completer.Reply(fit::ok(std::move(entries)));
}

void DeviceFsServer::Watch(WatchRequest& request,
                            WatchCompleter::Sync& completer) {
  auto server_end = std::move(request.watcher());

  std::lock_guard lock(watchers_mu_);
  watchers_.emplace_back(dispatcher_, std::move(server_end), this,
                         fidl::kIgnoreBindingClosure);

  fprintf(stderr, "driverhub: device_fs: Watch registered (total watchers: "
          "%zu)\n", watchers_.size());
}

void DeviceFsServer::ReadAttribute(ReadAttributeRequest& request,
                                    ReadAttributeCompleter::Sync& completer) {
  const std::string path(request.path());
  std::string prefix;

  if (!ClassifyPath(path, prefix)) {
    completer.Reply(fit::error(ZX_ERR_NOT_FOUND));
    return;
  }

  char buf[kAttrBufSize];
  ssize_t result = -1;

  if (prefix == "sys") {
    result = SysFs::Instance().ReadAttr(path, buf, sizeof(buf));
  } else if (prefix == "proc") {
    result = ProcFs::Instance().Read(path, buf, sizeof(buf));
  } else {
    // /dev paths are not readable as attributes; use OpenDevice instead.
    completer.Reply(fit::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }

  if (result < 0) {
    completer.Reply(fit::error(ZX_ERR_IO));
    return;
  }

  std::vector<uint8_t> content(buf, buf + result);
  completer.Reply(fit::ok(std::move(content)));
}

void DeviceFsServer::WriteAttribute(WriteAttributeRequest& request,
                                     WriteAttributeCompleter::Sync& completer) {
  const std::string path(request.path());
  const auto& content = request.content();
  std::string prefix;

  if (!ClassifyPath(path, prefix)) {
    completer.Reply(fit::error(ZX_ERR_NOT_FOUND));
    return;
  }

  ssize_t result = -1;

  if (prefix == "sys") {
    result = SysFs::Instance().WriteAttr(
        path, reinterpret_cast<const char*>(content.data()), content.size());
  } else if (prefix == "proc") {
    result = ProcFs::Instance().Write(
        path, reinterpret_cast<const char*>(content.data()), content.size());
  } else {
    completer.Reply(fit::error(ZX_ERR_NOT_SUPPORTED));
    return;
  }

  if (result < 0) {
    completer.Reply(fit::error(ZX_ERR_IO));
    return;
  }

  completer.Reply(fit::ok());
}

void DeviceFsServer::OpenDevice(OpenDeviceRequest& request,
                                 OpenDeviceCompleter::Sync& completer) {
  const std::string path(request.path());
  uint32_t flags = request.flags();
  (void)flags;  // Flags reserved for future use (O_RDONLY, O_RDWR, etc.).

  int fd = DevFs::Instance().Open(path);
  if (fd < 0) {
    completer.Reply(fit::error(ZX_ERR_NOT_FOUND));
    return;
  }

  completer.Reply(fit::ok(static_cast<uint32_t>(fd)));
}

void DeviceFsServer::NotifyEvent(fuchsia_driverhub::FsEvent event) {
  std::lock_guard lock(watchers_mu_);

  // Remove closed watchers and send event to active ones.
  auto it = watchers_.begin();
  while (it != watchers_.end()) {
    auto result = fidl::SendEvent(*it)->OnEvent(event);
    if (result.is_error()) {
      fprintf(stderr, "driverhub: device_fs: removing closed watcher\n");
      it = watchers_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace driverhub
