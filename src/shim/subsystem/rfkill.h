// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_RFKILL_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_RFKILL_H_

// DriverHub rfkill service layer.
//
// Tracks RF kill switch state as radios are registered via rfkill.ko's
// exported API (rfkill_alloc, rfkill_register, rfkill_set_sw_state, etc.).
// Exposes a query/control interface that maps to the fuchsia.driverhub.Rfkill
// FIDL protocol.
//
// On Fuchsia, this will serve the FIDL protocol directly.
// In QEMU standalone mode, the rfkillctl client tool queries this layer
// via a simple command protocol over a Unix-domain socket at /tmp/rfkill.sock.

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace driverhub {

// Radio type constants (match Linux RFKILL_TYPE_*).
enum class RfkillType : uint8_t {
  kAll = 0,
  kWlan = 1,
  kBluetooth = 2,
  kUwb = 3,
  kWimax = 4,
  kWwan = 5,
  kGps = 6,
  kFm = 7,
  kNfc = 8,
};

const char* RfkillTypeName(RfkillType type);

struct RfkillState {
  bool soft_blocked = false;
  bool hard_blocked = false;
};

struct RadioInfo {
  uint32_t index;
  RfkillType type;
  std::string name;
  RfkillState state;
};

// Callback for state-change events.
using RfkillEventCallback = std::function<void(const RadioInfo&)>;

// Global rfkill service.  Singleton — accessed by the shim functions
// (extern "C") and by the IPC server.
class RfkillService {
 public:
  static RfkillService& Instance();

  // Register a new radio.  Returns the assigned index.
  uint32_t RegisterRadio(RfkillType type, const std::string& name);

  // Unregister a radio by index.
  void UnregisterRadio(uint32_t index);

  // Set software block state for a single radio.
  bool SetSoftBlock(uint32_t index, bool blocked);

  // Set software block for all radios of a given type (kAll = all).
  void SetSoftBlockAll(RfkillType type, bool blocked);

  // Set hardware block state (driven by rfkill_set_hw_state).
  bool SetHardBlock(uint32_t index, bool blocked);

  // Get all registered radios.
  std::vector<RadioInfo> GetRadios() const;

  // Get a single radio by index.
  bool GetRadio(uint32_t index, RadioInfo* out) const;

  // Set event callback (for IPC notification).
  void SetEventCallback(RfkillEventCallback cb);

 private:
  RfkillService() = default;

  mutable std::mutex mu_;
  std::vector<RadioInfo> radios_;
  uint32_t next_index_ = 0;
  RfkillEventCallback event_cb_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_RFKILL_H_
