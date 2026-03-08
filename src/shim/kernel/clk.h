// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_CLK_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_CLK_H_

// KMI shims for Linux clock, regulator, and PM runtime APIs.
//
// Provides: clk_get, clk_prepare_enable, clk_disable_unprepare,
//           clk_get_rate, clk_set_rate,
//           regulator_get, regulator_enable, regulator_disable,
//           pm_runtime_enable, pm_runtime_get_sync, pm_runtime_put,
//           pm_runtime_put_sync, pm_runtime_set_active
//
// On Fuchsia:
//   - clk → fuchsia.hardware.clock/Clock FIDL
//   - regulator → fuchsia.hardware.vreg/Vreg FIDL
//   - PM runtime → stub (devices always on in userspace)

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct device;

// --- Clock API ---

struct clk;

struct clk* clk_get(struct device* dev, const char* id);
struct clk* devm_clk_get(struct device* dev, const char* id);
struct clk* devm_clk_get_optional(struct device* dev, const char* id);
void clk_put(struct clk* clk);

int clk_prepare(struct clk* clk);
void clk_unprepare(struct clk* clk);
int clk_enable(struct clk* clk);
void clk_disable(struct clk* clk);

int clk_prepare_enable(struct clk* clk);
void clk_disable_unprepare(struct clk* clk);

unsigned long clk_get_rate(struct clk* clk);
int clk_set_rate(struct clk* clk, unsigned long rate);
long clk_round_rate(struct clk* clk, unsigned long rate);

// --- Regulator API ---

struct regulator;

struct regulator* regulator_get(struct device* dev, const char* id);
struct regulator* devm_regulator_get(struct device* dev, const char* id);
struct regulator* devm_regulator_get_optional(struct device* dev,
                                               const char* id);
void regulator_put(struct regulator* reg);

int regulator_enable(struct regulator* reg);
int regulator_disable(struct regulator* reg);
int regulator_is_enabled(struct regulator* reg);

int regulator_set_voltage(struct regulator* reg, int min_uV, int max_uV);
int regulator_get_voltage(struct regulator* reg);

int regulator_set_load(struct regulator* reg, int uA);

// --- PM runtime API ---

void pm_runtime_enable(struct device* dev);
void pm_runtime_disable(struct device* dev);
int pm_runtime_get_sync(struct device* dev);
int pm_runtime_put(struct device* dev);
int pm_runtime_put_sync(struct device* dev);
void pm_runtime_set_active(struct device* dev);
void pm_runtime_set_autosuspend_delay(struct device* dev, int delay_ms);
void pm_runtime_use_autosuspend(struct device* dev);
int pm_runtime_resume_and_get(struct device* dev);
void pm_runtime_get_noresume(struct device* dev);
void pm_runtime_put_noidle(struct device* dev);
int pm_runtime_suspended(struct device* dev);

// --- clk_hw API (clock provider) ---

struct clk_hw;

struct clk_hw* __clk_hw_register_gate(struct device* dev, const char* name,
                                       const char* parent_name,
                                       unsigned long flags,
                                       void* reg, uint8_t bit_idx,
                                       uint8_t clk_gate_flags,
                                       void* lock);
struct clk_hw* __clk_hw_register_fixed_rate(struct device* dev,
                                             const char* name,
                                             const char* parent_name,
                                             unsigned long flags,
                                             unsigned long fixed_rate);
const char* clk_hw_get_name(const struct clk_hw* hw);
unsigned long clk_hw_get_flags(const struct clk_hw* hw);
struct clk_hw* clk_hw_get_parent(struct clk_hw* hw);
unsigned long clk_hw_get_rate(const struct clk_hw* hw);
int clk_hw_is_enabled(const struct clk_hw* hw);
int clk_hw_is_prepared(const struct clk_hw* hw);
void clk_hw_unregister_gate(struct clk_hw* hw);
void clk_hw_unregister_fixed_rate(struct clk_hw* hw);

// --- PM (suspend/resume) ---

// Power management message type (simplified).
typedef struct {
  int event;
} pm_message_t;

#define PM_EVENT_SUSPEND  0x0002
#define PM_EVENT_RESUME   0x0010
#define PM_EVENT_FREEZE   0x0001
#define PM_EVENT_THAW     0x0020

// dev_pm_ops used in driver structs.
struct dev_pm_ops {
  int (*suspend)(struct device*);
  int (*resume)(struct device*);
  int (*freeze)(struct device*);
  int (*thaw)(struct device*);
  int (*poweroff)(struct device*);
  int (*restore)(struct device*);
  int (*runtime_suspend)(struct device*);
  int (*runtime_resume)(struct device*);
  int (*runtime_idle)(struct device*);
};

// SET_SYSTEM_SLEEP_PM_OPS and SET_RUNTIME_PM_OPS — commonly used by UFS.
#define SET_SYSTEM_SLEEP_PM_OPS(suspend_fn, resume_fn) \
  .suspend = suspend_fn, \
  .resume = resume_fn, \
  .freeze = suspend_fn, \
  .thaw = resume_fn, \
  .poweroff = suspend_fn, \
  .restore = resume_fn

#define SET_RUNTIME_PM_OPS(suspend_fn, resume_fn, idle_fn) \
  .runtime_suspend = suspend_fn, \
  .runtime_resume = resume_fn, \
  .runtime_idle = idle_fn

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_CLK_H_
