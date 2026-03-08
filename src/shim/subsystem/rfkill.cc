// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/rfkill.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace driverhub {

const char* RfkillTypeName(RfkillType type) {
  switch (type) {
    case RfkillType::kAll:       return "all";
    case RfkillType::kWlan:      return "wlan";
    case RfkillType::kBluetooth: return "bluetooth";
    case RfkillType::kUwb:       return "uwb";
    case RfkillType::kWimax:     return "wimax";
    case RfkillType::kWwan:      return "wwan";
    case RfkillType::kGps:       return "gps";
    case RfkillType::kFm:        return "fm";
    case RfkillType::kNfc:       return "nfc";
    default:                     return "unknown";
  }
}

RfkillService& RfkillService::Instance() {
  static RfkillService instance;
  return instance;
}

uint32_t RfkillService::RegisterRadio(RfkillType type,
                                       const std::string& name) {
  std::lock_guard<std::mutex> lock(mu_);
  uint32_t idx = next_index_++;
  RadioInfo info;
  info.index = idx;
  info.type = type;
  info.name = name;
  radios_.push_back(info);
  fprintf(stderr, "driverhub: rfkill: registered radio %u '%s' type=%s\n",
          idx, name.c_str(), RfkillTypeName(type));
  return idx;
}

void RfkillService::UnregisterRadio(uint32_t index) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto it = radios_.begin(); it != radios_.end(); ++it) {
    if (it->index == index) {
      fprintf(stderr, "driverhub: rfkill: unregistered radio %u '%s'\n",
              index, it->name.c_str());
      radios_.erase(it);
      return;
    }
  }
}

bool RfkillService::SetSoftBlock(uint32_t index, bool blocked) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& r : radios_) {
    if (r.index == index) {
      r.state.soft_blocked = blocked;
      fprintf(stderr, "driverhub: rfkill: radio %u soft_block=%s\n",
              index, blocked ? "true" : "false");
      if (event_cb_) event_cb_(r);
      return true;
    }
  }
  return false;
}

void RfkillService::SetSoftBlockAll(RfkillType type, bool blocked) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& r : radios_) {
    if (type == RfkillType::kAll || r.type == type) {
      r.state.soft_blocked = blocked;
      fprintf(stderr, "driverhub: rfkill: radio %u soft_block=%s\n",
              r.index, blocked ? "true" : "false");
      if (event_cb_) event_cb_(r);
    }
  }
}

bool RfkillService::SetHardBlock(uint32_t index, bool blocked) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& r : radios_) {
    if (r.index == index) {
      r.state.hard_blocked = blocked;
      fprintf(stderr, "driverhub: rfkill: radio %u hard_block=%s\n",
              index, blocked ? "true" : "false");
      if (event_cb_) event_cb_(r);
      return true;
    }
  }
  return false;
}

std::vector<RadioInfo> RfkillService::GetRadios() const {
  std::lock_guard<std::mutex> lock(mu_);
  return radios_;
}

bool RfkillService::GetRadio(uint32_t index, RadioInfo* out) const {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& r : radios_) {
    if (r.index == index) {
      *out = r;
      return true;
    }
  }
  return false;
}

void RfkillService::SetEventCallback(RfkillEventCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  event_cb_ = std::move(cb);
}

}  // namespace driverhub

// ============================================================
// extern "C" shim hooks
// ============================================================
// These are called by modules that use rfkill (WiFi, BT drivers).
// They delegate to the RfkillService singleton so state is tracked
// and exposed via the FIDL / IPC protocol.
//
// The rfkill.ko module itself manages its own internal data structures.
// These hooks are for modules that call rfkill_alloc/rfkill_register
// from the EXPORT_SYMBOL-resolved function pointers.  We intercept
// the calls to maintain a parallel registry for our FIDL service.
//
// NOTE: rfkill.ko's exported functions handle the actual logic.
// Our hooks are registered as interposition symbols that wrap them.

// For standalone operation (no rfkill.ko loaded), provide a minimal
// rfkill implementation that the service tracks directly.

namespace {

// Simple rfkill object — enough to satisfy callers.
struct shim_rfkill {
  uint32_t index;
  uint8_t type;
  char name[80];
  bool soft_blocked;
  bool hard_blocked;
  // The set_block callback from the allocating driver.
  int (*set_block)(void* data, bool blocked);
  void* data;
};

}  // namespace

extern "C" {

// rfkill_alloc — allocate a new rfkill switch.
// Matches: struct rfkill *rfkill_alloc(const char *name, struct device *parent,
//                                       enum rfkill_type type,
//                                       const struct rfkill_ops *ops,
//                                       void *ops_data);
void* shim_rfkill_alloc(const char* name, void* parent, int type,
                         const void* ops, void* ops_data) {
  (void)parent;
  auto* rf = static_cast<struct shim_rfkill*>(
      calloc(1, sizeof(struct shim_rfkill)));
  if (!rf) return nullptr;

  rf->type = static_cast<uint8_t>(type);
  if (name) {
    strncpy(rf->name, name, sizeof(rf->name) - 1);
  }

  // The ops struct's first member is the set_block callback.
  if (ops) {
    const auto* fp = static_cast<const void* const*>(ops);
    rf->set_block = reinterpret_cast<int(*)(void*, bool)>
        (const_cast<void*>(fp[0]));
  }
  rf->data = ops_data;

  fprintf(stderr, "driverhub: rfkill: alloc '%s' type=%d\n",
          rf->name, rf->type);
  return rf;
}

// rfkill_register — register an allocated rfkill switch.
int shim_rfkill_register(void* rfkill) {
  if (!rfkill) return -22;
  auto* rf = static_cast<struct shim_rfkill*>(rfkill);

  auto type = static_cast<driverhub::RfkillType>(rf->type);
  rf->index = driverhub::RfkillService::Instance().RegisterRadio(
      type, rf->name);
  return 0;
}

// rfkill_unregister — unregister an rfkill switch.
void shim_rfkill_unregister(void* rfkill) {
  if (!rfkill) return;
  auto* rf = static_cast<struct shim_rfkill*>(rfkill);
  driverhub::RfkillService::Instance().UnregisterRadio(rf->index);
}

// rfkill_destroy — free rfkill switch.
void shim_rfkill_destroy(void* rfkill) {
  free(rfkill);
}

// rfkill_set_sw_state — set the software block state.
bool shim_rfkill_set_sw_state(void* rfkill, bool blocked) {
  if (!rfkill) return false;
  auto* rf = static_cast<struct shim_rfkill*>(rfkill);
  rf->soft_blocked = blocked;
  driverhub::RfkillService::Instance().SetSoftBlock(rf->index, blocked);
  return blocked;
}

// rfkill_set_hw_state — set the hardware block state.
bool shim_rfkill_set_hw_state(void* rfkill, bool blocked) {
  if (!rfkill) return false;
  auto* rf = static_cast<struct shim_rfkill*>(rfkill);
  rf->hard_blocked = blocked;
  driverhub::RfkillService::Instance().SetHardBlock(rf->index, blocked);
  return blocked;
}

// rfkill_blocked — check if radio is blocked (soft OR hard).
bool shim_rfkill_blocked(void* rfkill) {
  if (!rfkill) return true;
  auto* rf = static_cast<struct shim_rfkill*>(rfkill);
  return rf->soft_blocked || rf->hard_blocked;
}

// rfkill_soft_blocked — check if radio is soft-blocked.
bool shim_rfkill_soft_blocked(void* rfkill) {
  if (!rfkill) return true;
  auto* rf = static_cast<struct shim_rfkill*>(rfkill);
  return rf->soft_blocked;
}

}  // extern "C"
