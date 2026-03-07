// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the lightweight .modinfo ELF section parser.

#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "src/loader/modinfo_parser.h"

namespace {

// ============================================================
// ParseModinfo tests (raw .modinfo section data)
// ============================================================

TEST(ModinfoParserTest, EmptySection) {
  driverhub::ModuleInfo info;
  driverhub::ParseModinfo(nullptr, 0, &info);
  EXPECT_TRUE(info.description.empty());
  EXPECT_TRUE(info.depends.empty());
}

TEST(ModinfoParserTest, SingleField) {
  const char data[] = "license=GPL\0";
  driverhub::ModuleInfo info;
  driverhub::ParseModinfo(reinterpret_cast<const uint8_t*>(data),
                          sizeof(data), &info);
  EXPECT_EQ(info.license, "GPL");
}

TEST(ModinfoParserTest, MultipleFields) {
  // Null-terminated key=value pairs back-to-back.
  const char data[] =
      "description=Test driver\0"
      "author=Test Author\0"
      "license=GPL v2\0"
      "vermagic=6.6.0 SMP preempt\0";
  driverhub::ModuleInfo info;
  driverhub::ParseModinfo(reinterpret_cast<const uint8_t*>(data),
                          sizeof(data), &info);
  EXPECT_EQ(info.description, "Test driver");
  EXPECT_EQ(info.author, "Test Author");
  EXPECT_EQ(info.license, "GPL v2");
  EXPECT_EQ(info.vermagic, "6.6.0 SMP preempt");
}

TEST(ModinfoParserTest, Dependencies) {
  const char data[] = "depends=i2c_core,regmap\0";
  driverhub::ModuleInfo info;
  driverhub::ParseModinfo(reinterpret_cast<const uint8_t*>(data),
                          sizeof(data), &info);
  ASSERT_EQ(info.depends.size(), 2u);
  EXPECT_EQ(info.depends[0], "i2c_core");
  EXPECT_EQ(info.depends[1], "regmap");
}

TEST(ModinfoParserTest, EmptyDepends) {
  const char data[] = "depends=\0";
  driverhub::ModuleInfo info;
  driverhub::ParseModinfo(reinterpret_cast<const uint8_t*>(data),
                          sizeof(data), &info);
  EXPECT_TRUE(info.depends.empty());
}

TEST(ModinfoParserTest, MultipleAliases) {
  const char data[] =
      "alias=of:N*T*Cvendor,deviceA\0"
      "alias=of:N*T*Cvendor,deviceB\0";
  driverhub::ModuleInfo info;
  driverhub::ParseModinfo(reinterpret_cast<const uint8_t*>(data),
                          sizeof(data), &info);
  ASSERT_EQ(info.aliases.size(), 2u);
  EXPECT_EQ(info.aliases[0], "of:N*T*Cvendor,deviceA");
  EXPECT_EQ(info.aliases[1], "of:N*T*Cvendor,deviceB");
}

TEST(ModinfoParserTest, UnknownKeysIgnored) {
  const char data[] =
      "srcversion=ABC123\0"
      "license=GPL\0"
      "retpoline=Y\0";
  driverhub::ModuleInfo info;
  driverhub::ParseModinfo(reinterpret_cast<const uint8_t*>(data),
                          sizeof(data), &info);
  EXPECT_EQ(info.license, "GPL");
  // Unknown keys should not cause failure.
}

TEST(ModinfoParserTest, NullBytePadding) {
  // Some .modinfo sections have extra null bytes between entries.
  const char data[] = "license=GPL\0\0\0author=Me\0";
  driverhub::ModuleInfo info;
  driverhub::ParseModinfo(reinterpret_cast<const uint8_t*>(data),
                          sizeof(data), &info);
  EXPECT_EQ(info.license, "GPL");
  EXPECT_EQ(info.author, "Me");
}

// ============================================================
// ExtractModuleInfo tests (full ELF parsing)
// ============================================================

TEST(ModinfoParserTest, InvalidElfTooSmall) {
  uint8_t data[4] = {0};
  driverhub::ModuleInfo info;
  EXPECT_FALSE(driverhub::ExtractModuleInfo(data, sizeof(data), &info));
}

TEST(ModinfoParserTest, InvalidElfBadMagic) {
  uint8_t data[128] = {0};
  data[0] = 'N';  // Not ELF magic.
  driverhub::ModuleInfo info;
  EXPECT_FALSE(driverhub::ExtractModuleInfo(data, sizeof(data), &info));
}

}  // namespace
