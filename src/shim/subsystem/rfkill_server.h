// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_RFKILL_SERVER_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_RFKILL_SERVER_H_

// Unix-domain socket IPC server for the rfkill service.
//
// Listens on /tmp/rfkill.sock (configurable) and handles simple
// text-based commands from the rfkillctl client tool:
//
//   LIST                     -> list all radios
//   BLOCK <index>            -> soft-block a radio
//   UNBLOCK <index>          -> soft-unblock a radio
//   BLOCKALL [type]          -> soft-block all radios (optionally by type)
//   UNBLOCKALL [type]        -> soft-unblock all radios
//
// On Fuchsia, this would be replaced by a FIDL protocol server.

#include <string>

namespace driverhub {

// Start the rfkill IPC server on a background thread.
// `socket_path` defaults to /tmp/rfkill.sock if empty.
void StartRfkillServer(const std::string& socket_path = "");

// Stop the rfkill IPC server.
void StopRfkillServer();

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_RFKILL_SERVER_H_
