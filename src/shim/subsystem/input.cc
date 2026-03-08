// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/input.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// Input subsystem shim.
//
// Maintains simulated input device state. Tracks registered devices, absolute
// axis parameters, and multi-touch slot configuration.
//
// On Fuchsia, input events would be forwarded to the
// fuchsia.input.report/InputDevice FIDL protocol.

namespace {

std::mutex g_input_mu;
std::vector<struct input_dev*> g_input_devices;

}  // namespace

extern "C" {

// --- Input device lifecycle ---

struct input_dev* input_allocate_device(void) {
  auto* dev = static_cast<struct input_dev*>(
      calloc(1, sizeof(struct input_dev)));
  if (dev) {
    fprintf(stderr, "driverhub: input: allocated device\n");
  }
  return dev;
}

struct input_dev* devm_input_allocate_device(struct device* parent) {
  (void)parent;
  return input_allocate_device();
}

void input_free_device(struct input_dev* dev) {
  if (!dev) return;
  fprintf(stderr, "driverhub: input: freed device '%s'\n",
          dev->name ? dev->name : "");
  free(dev);
}

int input_register_device(struct input_dev* dev) {
  if (!dev) return -22;  // -EINVAL
  std::lock_guard<std::mutex> lock(g_input_mu);
  g_input_devices.push_back(dev);
  fprintf(stderr, "driverhub: input: registered '%s' (%d MT slots)\n",
          dev->name ? dev->name : "(unnamed)", dev->mt_num_slots);
  return 0;
}

void input_unregister_device(struct input_dev* dev) {
  if (!dev) return;
  std::lock_guard<std::mutex> lock(g_input_mu);
  for (auto it = g_input_devices.begin(); it != g_input_devices.end(); ++it) {
    if (*it == dev) {
      g_input_devices.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: input: unregistered '%s'\n",
          dev->name ? dev->name : "");
  free(dev);
}

// --- Event reporting ---

void input_event(struct input_dev* dev, unsigned int type,
                 unsigned int code, int value) {
  (void)dev;
  (void)type;
  (void)code;
  (void)value;
  // In a full implementation, this would forward events to the Fuchsia
  // input pipeline via FIDL.
}

void input_report_key(struct input_dev* dev, unsigned int code, int value) {
  input_event(dev, EV_KEY, code, value);
}

void input_report_rel(struct input_dev* dev, unsigned int code, int value) {
  input_event(dev, EV_REL, code, value);
}

void input_report_abs(struct input_dev* dev, unsigned int code, int value) {
  input_event(dev, EV_ABS, code, value);
}

void input_sync(struct input_dev* dev) {
  input_event(dev, EV_SYN, SYN_REPORT, 0);
}

void input_mt_sync(struct input_dev* dev) {
  input_event(dev, EV_SYN, SYN_MT_REPORT, 0);
}

// --- Multi-touch API ---

int input_mt_init_slots(struct input_dev* dev, unsigned int num_slots,
                        unsigned int flags) {
  if (!dev) return -22;
  dev->mt_num_slots = static_cast<int>(num_slots);
  dev->mt_flags = static_cast<int>(flags);
  // Set up tracking ID axis automatically.
  input_set_abs_params(dev, ABS_MT_SLOT, 0,
                       static_cast<int>(num_slots) - 1, 0, 0);
  input_set_abs_params(dev, ABS_MT_TRACKING_ID, 0, 65535, 0, 0);
  fprintf(stderr, "driverhub: input: MT init %u slots (flags=0x%x)\n",
          num_slots, flags);
  return 0;
}

void input_mt_slot(struct input_dev* dev, int slot) {
  input_report_abs(dev, ABS_MT_SLOT, slot);
}

void input_mt_report_slot_state(struct input_dev* dev, unsigned int tool_type,
                                int active) {
  (void)tool_type;
  input_report_abs(dev, ABS_MT_TRACKING_ID, active ? 0 : -1);
}

void input_mt_sync_frame(struct input_dev* dev) {
  // Signal end of multi-touch frame.
  input_sync(dev);
}

// --- Absolute axis configuration ---

void input_set_abs_params(struct input_dev* dev, unsigned int axis,
                          int min, int max, int fuzz, int flat) {
  if (!dev || axis > ABS_MAX) return;
  dev->absinfo[axis].minimum = min;
  dev->absinfo[axis].maximum = max;
  dev->absinfo[axis].fuzz = fuzz;
  dev->absinfo[axis].flat = flat;
  // Set the ABS bit.
  __set_bit(axis, dev->absbit);
  __set_bit(EV_ABS, dev->evbit);
}

// --- Capability bits ---

void input_set_capability(struct input_dev* dev, unsigned int type,
                          unsigned int code) {
  if (!dev) return;
  __set_bit(type, dev->evbit);
  switch (type) {
    case EV_KEY:
      __set_bit(code, dev->keybit);
      break;
    case EV_REL:
      __set_bit(code, dev->relbit);
      break;
    case EV_ABS:
      __set_bit(code, dev->absbit);
      break;
    default:
      break;
  }
}

// --- Input polling API (used by input_test.ko) ---

void input_setup_polling(struct input_dev* dev,
                          void (*poll)(struct input_dev*)) {
  (void)dev;
  (void)poll;
  fprintf(stderr, "driverhub: input: setup polling\n");
}

void input_set_poll_interval(struct input_dev* dev, unsigned int interval_ms) {
  (void)dev;
  (void)interval_ms;
}

unsigned int input_get_poll_interval(struct input_dev* dev) {
  (void)dev;
  return 0;
}

void input_set_timestamp(struct input_dev* dev, long long timestamp) {
  (void)dev;
  (void)timestamp;
}

long long input_get_timestamp(struct input_dev* dev) {
  (void)dev;
  return 0;
}

int input_grab_device(void* handle) {
  (void)handle;
  return 0;
}

void input_release_device(void* handle) {
  (void)handle;
}

int input_match_device_id(const struct input_dev* dev, const void* id) {
  (void)dev;
  (void)id;
  return 1;  // Match.
}

// get_device: increment device refcount (generic kernel API).
struct device* get_device(struct device* dev) {
  return dev;  // No-op refcount in shim.
}

}  // extern "C"
