// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_INCLUDE_LINUX_ANDROID_KABI_H_
#define DRIVERHUB_SRC_SHIM_INCLUDE_LINUX_ANDROID_KABI_H_

// ANDROID_KABI_RESERVE / ANDROID_KABI_USE / ANDROID_KABI_REPLACE macros.
//
// GKI kernel structs use these macros to reserve u64 padding slots at the
// end of ABI-frozen structs. When the ABI needs extending, ANDROID_KABI_USE
// replaces a reserved slot with a typed field of equal or smaller size.
//
// Our shim structs must include the same KABI reserve slots so that the
// overall struct size and field offsets match what pre-compiled .ko modules
// expect. Without these, struct sizes will be wrong and module code will
// access memory at incorrect offsets.
//
// Reference: https://source.android.com/docs/core/architecture/kernel/abi-monitor

#include <stdint.h>

// Reserve a u64 padding slot. n is a unique index within the struct.
#define ANDROID_KABI_RESERVE(n) uint64_t android_kabi_reserved##n

// Consume a previously reserved slot with a new field.
// The replacement must fit within u64 (8 bytes).
#define ANDROID_KABI_USE(n, replacement) replacement

// Consume a reserved slot, replacing it with two fields packed into u64.
#define ANDROID_KABI_USE2(n, replacement1, replacement2) \
  replacement1;                                          \
  replacement2

// Replace an existing field with a different type (same size).
#define ANDROID_KABI_REPLACE(orig, replacement) replacement

#endif  // DRIVERHUB_SRC_SHIM_INCLUDE_LINUX_ANDROID_KABI_H_
