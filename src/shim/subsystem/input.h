// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_INPUT_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_INPUT_H_

// KMI shims for the Linux input subsystem APIs (touch, buttons, etc.).
//
// Provides: input_allocate_device, input_register_device,
//           input_unregister_device, input_free_device,
//           input_report_abs, input_report_key, input_sync,
//           input_mt_init_slots, input_mt_slot, input_mt_report_slot_state,
//           input_mt_sync_frame, input_set_abs_params, etc.
//
// On Fuchsia: maps to fuchsia.input.report FIDL protocol.

#include <stddef.h>
#include <stdint.h>

#include "src/shim/kernel/device.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Event types ---
#define EV_SYN       0x00
#define EV_KEY       0x01
#define EV_REL       0x02
#define EV_ABS       0x03
#define EV_MSC       0x04
#define EV_SW        0x05
#define EV_LED       0x11
#define EV_REP       0x14

// --- Key/button codes ---
#define BTN_TOUCH    0x14a
#define BTN_TOOL_FINGER  0x145
#define BTN_TOOL_PEN     0x140
#define BTN_LEFT     0x110
#define BTN_RIGHT    0x111
#define BTN_MIDDLE   0x112
#define KEY_POWER    116
#define KEY_VOLUMEUP 115
#define KEY_VOLUMEDOWN 114
#define KEY_HOME     102
#define KEY_BACK     158

// --- Absolute axis codes ---
#define ABS_X        0x00
#define ABS_Y        0x01
#define ABS_Z        0x02
#define ABS_PRESSURE 0x18
#define ABS_MT_SLOT           0x2f
#define ABS_MT_TOUCH_MAJOR    0x30
#define ABS_MT_TOUCH_MINOR    0x31
#define ABS_MT_WIDTH_MAJOR    0x32
#define ABS_MT_WIDTH_MINOR    0x33
#define ABS_MT_POSITION_X     0x35
#define ABS_MT_POSITION_Y     0x36
#define ABS_MT_TRACKING_ID    0x39
#define ABS_MT_PRESSURE       0x3a
#define ABS_MT_DISTANCE       0x3b

// --- Relative axis codes ---
#define REL_X        0x00
#define REL_Y        0x01
#define REL_WHEEL    0x08

// --- Misc ---
#define SYN_REPORT   0
#define SYN_MT_REPORT 2

// --- Input device properties ---
#define INPUT_PROP_DIRECT       0x01
#define INPUT_PROP_POINTER      0x02
#define INPUT_PROP_BUTTONPAD    0x05

// --- Multi-touch flags ---
#define INPUT_MT_DIRECT     0x0001
#define INPUT_MT_POINTER    0x0002
#define INPUT_MT_DROP_UNUSED 0x0004
#define INPUT_MT_TRACK       0x0008

// Bit array size (enough for max event codes).
#define BITS_TO_LONGS(nr)  (((nr) + 63) / 64)
#define EV_MAX    0x1f
#define KEY_MAX   0x2ff
#define ABS_MAX   0x3f
#define REL_MAX   0x0f

// --- Input device ID ---
struct input_id {
  uint16_t bustype;
  uint16_t vendor;
  uint16_t product;
  uint16_t version;
};

#define BUS_I2C       0x18
#define BUS_SPI       0x1c
#define BUS_USB       0x03
#define BUS_HOST      0x19
#define BUS_VIRTUAL   0x06

// --- Absolute axis info ---
struct input_absinfo {
  int value;
  int minimum;
  int maximum;
  int fuzz;
  int flat;
  int resolution;
};

// --- Input device ---
struct input_dev {
  const char *name;
  const char *phys;
  const char *uniq;
  struct input_id id;
  struct device dev;

  unsigned long evbit[BITS_TO_LONGS(EV_MAX + 1)];
  unsigned long keybit[BITS_TO_LONGS(KEY_MAX + 1)];
  unsigned long relbit[BITS_TO_LONGS(REL_MAX + 1)];
  unsigned long absbit[BITS_TO_LONGS(ABS_MAX + 1)];
  unsigned long propbit[2];

  struct input_absinfo absinfo[ABS_MAX + 1];

  // Multi-touch slot state.
  int mt_num_slots;
  int mt_flags;

  // Open/close callbacks.
  int (*open)(struct input_dev *dev);
  void (*close)(struct input_dev *dev);

  // Private data for driver use.
  void *private_data;
};

// --- Input device lifecycle ---
struct input_dev *input_allocate_device(void);
struct input_dev *devm_input_allocate_device(struct device *dev);
void input_free_device(struct input_dev *dev);
int input_register_device(struct input_dev *dev);
void input_unregister_device(struct input_dev *dev);

// --- Event reporting ---
void input_event(struct input_dev *dev, unsigned int type,
                 unsigned int code, int value);
void input_report_key(struct input_dev *dev, unsigned int code, int value);
void input_report_rel(struct input_dev *dev, unsigned int code, int value);
void input_report_abs(struct input_dev *dev, unsigned int code, int value);
void input_sync(struct input_dev *dev);
void input_mt_sync(struct input_dev *dev);

// --- Multi-touch API ---
int input_mt_init_slots(struct input_dev *dev, unsigned int num_slots,
                        unsigned int flags);
void input_mt_slot(struct input_dev *dev, int slot);
void input_mt_report_slot_state(struct input_dev *dev, unsigned int tool_type,
                                int active);
void input_mt_sync_frame(struct input_dev *dev);

// --- Absolute axis configuration ---
void input_set_abs_params(struct input_dev *dev, unsigned int axis,
                          int min, int max, int fuzz, int flat);

// --- Capability bits ---
void input_set_capability(struct input_dev *dev, unsigned int type,
                          unsigned int code);

// --- Helper to set device data ---
static inline void input_set_drvdata(struct input_dev *dev, void *data) {
  dev->private_data = data;
}

static inline void *input_get_drvdata(struct input_dev *dev) {
  return dev->private_data;
}

// --- Property bits ---
static inline void __set_bit(unsigned int bit, unsigned long *addr) {
  addr[bit / (sizeof(unsigned long) * 8)] |=
      1UL << (bit % (sizeof(unsigned long) * 8));
}

static inline int test_bit(unsigned int bit, const unsigned long *addr) {
  return (addr[bit / (sizeof(unsigned long) * 8)] >>
          (bit % (sizeof(unsigned long) * 8))) & 1;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_INPUT_H_
