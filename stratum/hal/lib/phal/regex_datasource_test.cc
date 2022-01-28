// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/regex_datasource.h"

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/fixed_stringsource.h"
#include "stratum/hal/lib/phal/test_util.h"
#include "stratum/lib/test_utils/matchers.h"

namespace stratum {
namespace hal {
namespace phal {
namespace {

TEST(RegexDataSourceTest, UpdateSucceedsIfRegexMatches) {
  auto input = absl::make_unique<FixedStringSource>("testing");
  auto datasource =
      RegexDataSource::Make("\\w+", std::move(input), new NoCache());
  EXPECT_OK(datasource->UpdateValues());
}

TEST(RegexDataSourceTest, UpdateFailsIfRegexFails) {
  auto input = absl::make_unique<FixedStringSource>("testing failure");
  // Our regex won't match the space character.
  auto datasource =
      RegexDataSource::Make("\\w+", std::move(input), new NoCache());
  EXPECT_FALSE(datasource->UpdateValues().ok());
}

TEST(RegexDataSourceTest, CanIgnoreMatchGroups) {
  auto input = absl::make_unique<FixedStringSource>("testing 12345");
  auto datasource =
      RegexDataSource::Make("(\\w+) (\\d+)", std::move(input), new NoCache());
  EXPECT_OK(datasource->UpdateValues());
}

TEST(RegexDataSourceTest, CanReadWholeStringFromRegex) {
  auto input = absl::make_unique<FixedStringSource>("testing");
  auto datasource =
      RegexDataSource::Make("(\\w+)", std::move(input), new NoCache());
  auto attribute = datasource->GetAttribute<std::string>(1);
  ASSERT_TRUE(attribute.ok());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(attribute, IsOkAndContainsValue<std::string>("testing"));
}

TEST(RegexDataSourceTest, CanReadIntegerFromRegex) {
  auto input = absl::make_unique<FixedStringSource>("12345");
  auto datasource =
      RegexDataSource::Make("(\\d+)", std::move(input), new NoCache());
  auto attribute = datasource->GetAttribute<uint32>(1);
  ASSERT_TRUE(attribute.ok());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(attribute, IsOkAndContainsValue<uint32>(12345));
}

TEST(RegexDataSourceTest, CanReadSignedIntegerFromRegex) {
  auto input = absl::make_unique<FixedStringSource>("-12345");
  auto datasource =
      RegexDataSource::Make("(-?\\d+)", std::move(input), new NoCache());
  auto attribute = datasource->GetAttribute<int32>(1);
  ASSERT_TRUE(attribute.ok());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(attribute, IsOkAndContainsValue<int32>(-12345));
}

TEST(RegexDataSourceTest, CanReadMultipleFieldsFromRegex) {
  auto input = absl::make_unique<FixedStringSource>("testing 12345");
  auto datasource =
      RegexDataSource::Make("(\\w+) (\\d+)", std::move(input), new NoCache());
  auto str_attribute = datasource->GetAttribute<std::string>(1);
  auto int_attribute = datasource->GetAttribute<int32>(2);
  ASSERT_TRUE(str_attribute.ok());
  ASSERT_TRUE(int_attribute.ok());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(str_attribute, IsOkAndContainsValue<std::string>("testing"));
  EXPECT_THAT(int_attribute, IsOkAndContainsValue<int32>(12345));
}

TEST(RegexDataSourceTest, CanSkipFieldsFromRegex) {
  auto input = absl::make_unique<FixedStringSource>("testing 12345");
  auto datasource =
      RegexDataSource::Make("(\\w+) (\\d+)", std::move(input), new NoCache());
  auto int_attribute = datasource->GetAttribute<int32>(2);
  ASSERT_TRUE(int_attribute.ok());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(int_attribute, IsOkAndContainsValue<int32>(12345));
}

TEST(RegexDataSourceTest, UpdateFailsIfFieldContentsNotInteger) {
  auto input = absl::make_unique<FixedStringSource>("not_a_number");
  auto datasource =
      RegexDataSource::Make("(.+)", std::move(input), new NoCache());
  auto attribute = datasource->GetAttribute<int64>(1);
  ASSERT_TRUE(attribute.ok());
  EXPECT_FALSE(datasource->UpdateValues().ok());
}

TEST(RegexDataSourceTest, CannotRequestInvalidCaptureGroup) {
  auto input = absl::make_unique<FixedStringSource>("testing");
  auto datasource =
      RegexDataSource::Make("(.+)", std::move(input), new NoCache());
  EXPECT_FALSE(datasource->GetAttribute<std::string>(-1).ok());
  EXPECT_FALSE(datasource->GetAttribute<std::string>(0).ok());
  EXPECT_FALSE(datasource->GetAttribute<std::string>(2).ok());
}

TEST(RegexDataSourceTest, CannotRequestCaptureGroupTwice) {
  auto input = absl::make_unique<FixedStringSource>("testing");
  auto datasource =
      RegexDataSource::Make("(.+)", std::move(input), new NoCache());
  auto attribute = datasource->GetAttribute<std::string>(1);
  EXPECT_TRUE(attribute.ok());
  EXPECT_FALSE(datasource->GetAttribute<std::string>(1).ok());
}

TEST(RegexDataSourceTest, CanReadMultilineText) {
  auto input = absl::make_unique<FixedStringSource>("blah blah 123\nfoo 456.");
  auto datasource = RegexDataSource::Make("\\D+(\\d+)\\D+(\\d+)\\D+",
                                          std::move(input), new NoCache());
  auto attribute1 = datasource->GetAttribute<int32>(1);
  auto attribute2 = datasource->GetAttribute<int32>(2);
  ASSERT_TRUE(attribute1.ok());
  ASSERT_TRUE(attribute2.ok());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(attribute1, IsOkAndContainsValue<int32>(123));
  EXPECT_THAT(attribute2, IsOkAndContainsValue<int32>(456));
}

}  // namespace
}  // namespace phal
}  // namespace hal
}  // namespace stratum
