// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/kernel/clk.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Clock, regulator, and PM runtime shims.
//
// In userspace, clocks and regulators are simulated (always enabled,
// fixed rate). PM runtime calls are no-ops — the device is always "on".
//
// On Fuchsia:
//   - clk_get → fuchsia.hardware.clock/Clock FIDL lookup
//   - regulator_get → fuchsia.hardware.vreg/Vreg FIDL lookup
//   - PM runtime → tracked state with optional suspend/resume callbacks

namespace {

// Simulated clock state.
struct clk_impl {
  char name[64];
  unsigned long rate;
  int enabled;
  int prepared;
};

// Simulated regulator state.
struct regulator_impl {
  char name[64];
  int enabled;
  int voltage_uV;
};

}  // namespace

extern "C" {

// --- Clock API ---

struct clk* clk_get(struct device* dev, const char* id) {
  (void)dev;
  auto* clk = static_cast<struct clk_impl*>(calloc(1, sizeof(clk_impl)));
  if (!clk)
    return reinterpret_cast<struct clk*>((long)-12);  // ERR_PTR(-ENOMEM)

  snprintf(clk->name, sizeof(clk->name), "%s", id ? id : "default");
  clk->rate = 200000000;  // 200 MHz default.

  fprintf(stderr, "driverhub: clk: get '%s' (simulated, rate=%lu)\n",
          clk->name, clk->rate);
  return reinterpret_cast<struct clk*>(clk);
}

struct clk* devm_clk_get(struct device* dev, const char* id) {
  return clk_get(dev, id);
}

struct clk* devm_clk_get_optional(struct device* dev, const char* id) {
  // Optional: return NULL instead of error if not found.
  // We always provide a simulated clock.
  return clk_get(dev, id);
}

void clk_put(struct clk* clk) {
  if (clk && !((long)clk < 0 && (long)clk > -4096)) {
    free(clk);
  }
}

int clk_prepare(struct clk* clk) {
  if (!clk) return -22;
  auto* impl = reinterpret_cast<clk_impl*>(clk);
  impl->prepared = 1;
  return 0;
}

void clk_unprepare(struct clk* clk) {
  if (!clk) return;
  auto* impl = reinterpret_cast<clk_impl*>(clk);
  impl->prepared = 0;
}

int clk_enable(struct clk* clk) {
  if (!clk) return -22;
  auto* impl = reinterpret_cast<clk_impl*>(clk);
  impl->enabled = 1;
  return 0;
}

void clk_disable(struct clk* clk) {
  if (!clk) return;
  auto* impl = reinterpret_cast<clk_impl*>(clk);
  impl->enabled = 0;
}

int clk_prepare_enable(struct clk* clk) {
  int ret = clk_prepare(clk);
  if (ret) return ret;
  return clk_enable(clk);
}

void clk_disable_unprepare(struct clk* clk) {
  clk_disable(clk);
  clk_unprepare(clk);
}

unsigned long clk_get_rate(struct clk* clk) {
  if (!clk) return 0;
  return reinterpret_cast<clk_impl*>(clk)->rate;
}

int clk_set_rate(struct clk* clk, unsigned long rate) {
  if (!clk) return -22;
  auto* impl = reinterpret_cast<clk_impl*>(clk);
  fprintf(stderr, "driverhub: clk: set_rate '%s' %lu -> %lu\n",
          impl->name, impl->rate, rate);
  impl->rate = rate;
  return 0;
}

long clk_round_rate(struct clk* clk, unsigned long rate) {
  (void)clk;
  return static_cast<long>(rate);  // Accept any rate.
}

// --- Regulator API ---

struct regulator* regulator_get(struct device* dev, const char* id) {
  (void)dev;
  auto* reg = static_cast<regulator_impl*>(calloc(1, sizeof(regulator_impl)));
  if (!reg)
    return reinterpret_cast<struct regulator*>((long)-12);

  snprintf(reg->name, sizeof(reg->name), "%s", id ? id : "default");
  reg->voltage_uV = 3300000;  // 3.3V default.

  fprintf(stderr, "driverhub: regulator: get '%s' (simulated)\n", reg->name);
  return reinterpret_cast<struct regulator*>(reg);
}

struct regulator* devm_regulator_get(struct device* dev, const char* id) {
  return regulator_get(dev, id);
}

struct regulator* devm_regulator_get_optional(struct device* dev,
                                               const char* id) {
  return regulator_get(dev, id);
}

void regulator_put(struct regulator* reg) {
  if (reg && !((long)reg < 0 && (long)reg > -4096)) {
    free(reg);
  }
}

int regulator_enable(struct regulator* reg) {
  if (!reg) return -22;
  auto* impl = reinterpret_cast<regulator_impl*>(reg);
  impl->enabled = 1;
  fprintf(stderr, "driverhub: regulator: enable '%s'\n", impl->name);
  return 0;
}

int regulator_disable(struct regulator* reg) {
  if (!reg) return -22;
  auto* impl = reinterpret_cast<regulator_impl*>(reg);
  impl->enabled = 0;
  fprintf(stderr, "driverhub: regulator: disable '%s'\n", impl->name);
  return 0;
}

int regulator_is_enabled(struct regulator* reg) {
  if (!reg) return 0;
  return reinterpret_cast<regulator_impl*>(reg)->enabled;
}

int regulator_set_voltage(struct regulator* reg, int min_uV, int max_uV) {
  if (!reg) return -22;
  auto* impl = reinterpret_cast<regulator_impl*>(reg);
  impl->voltage_uV = min_uV;
  fprintf(stderr, "driverhub: regulator: set_voltage '%s' %d-%d uV\n",
          impl->name, min_uV, max_uV);
  return 0;
}

int regulator_get_voltage(struct regulator* reg) {
  if (!reg) return -22;
  return reinterpret_cast<regulator_impl*>(reg)->voltage_uV;
}

int regulator_set_load(struct regulator* reg, int uA) {
  (void)reg;
  (void)uA;
  return 0;
}

// --- PM runtime ---
// In userspace, devices are always powered on. These are no-ops.

void pm_runtime_enable(struct device* dev) {
  (void)dev;
}

void pm_runtime_disable(struct device* dev) {
  (void)dev;
}

int pm_runtime_get_sync(struct device* dev) {
  (void)dev;
  return 0;  // Success, device is active.
}

int pm_runtime_put(struct device* dev) {
  (void)dev;
  return 0;
}

int pm_runtime_put_sync(struct device* dev) {
  (void)dev;
  return 0;
}

void pm_runtime_set_active(struct device* dev) {
  (void)dev;
}

void pm_runtime_set_autosuspend_delay(struct device* dev, int delay_ms) {
  (void)dev;
  (void)delay_ms;
}

void pm_runtime_use_autosuspend(struct device* dev) {
  (void)dev;
}

int pm_runtime_resume_and_get(struct device* dev) {
  (void)dev;
  return 0;
}

void pm_runtime_get_noresume(struct device* dev) {
  (void)dev;
}

void pm_runtime_put_noidle(struct device* dev) {
  (void)dev;
}

int pm_runtime_suspended(struct device* dev) {
  (void)dev;
  return 0;  // Not suspended.
}

}  // extern "C"
