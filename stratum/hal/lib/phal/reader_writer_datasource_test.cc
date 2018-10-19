// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "stratum/hal/lib/phal/reader_writer_datasource.h"

#include <memory>

#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/filepath_stringsource.h"
#include "stratum/hal/lib/phal/system_fake.h"
#include "stratum/hal/lib/phal/test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace hal {
namespace phal {
namespace {

using ::testing::_;
using ::testing::ContainsRegex;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

// Note that the filesystem is faked in all of these tests, so this file is
// never actually created.
const char kTestFilepath[] = "/tmp/test_file.txt";
const char kTestFileContents[] = "FILE CONTENTS";
const char kTestReplacementFileContents[] = "REPLACEMENT FILE CONTENTS";

TEST(ReaderWriterDataSourceTest, CanReadUnwritableFile) {
  SystemFake system_interface;
  system_interface.AddFakeFile(kTestFilepath, kTestFileContents);

  auto datasource = ReaderWriterDataSource<std::string>::Make(
      absl::make_unique<FilepathStringSource>(&system_interface, kTestFilepath),
      new NoCache());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(datasource->GetAttribute(),
              ContainsValue<std::string>(kTestFileContents));
}

TEST(ReaderWriterDataSourceTest, ReadFromMissingFileFails) {
  SystemFake system_interface;

  auto datasource = ReaderWriterDataSource<std::string>::Make(
      absl::make_unique<FilepathStringSource>(&system_interface, kTestFilepath),
      new NoCache());
  EXPECT_THAT(
      datasource->UpdateValues(),
      StatusIs(_, _, ContainsRegex("Cannot read file.* Does not exist")));
}

TEST(ReaderWriterDataSourceTest, CannotWriteToUnwritableFile) {
  SystemFake system_interface;
  system_interface.AddFakeFile(kTestFilepath, kTestFileContents);
  auto datasource = ReaderWriterDataSource<std::string>::Make(
      absl::make_unique<FilepathStringSource>(&system_interface, kTestFilepath),
      new NoCache());
  EXPECT_FALSE(datasource->GetAttribute()->CanSet());
  EXPECT_THAT(datasource->GetAttribute()->Set(
                  std::string(kTestReplacementFileContents)),
              StatusIs(_, _, HasSubstr("Selected attribute cannot be set")));
}

TEST(ReaderWriterDataSourceTest, CanWriteToWritableFile) {
  SystemFake system_interface;
  system_interface.AddFakeFile(kTestFilepath, kTestFileContents);
  auto datasource = ReaderWriterDataSource<std::string>::Make(
      absl::make_unique<FilepathStringSource>(&system_interface, kTestFilepath,
                                              true),
      new NoCache());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(datasource->GetAttribute(),
              ContainsValue<std::string>(kTestFileContents));
  // Write to the system.
  EXPECT_TRUE(datasource->GetAttribute()->CanSet());
  EXPECT_OK(datasource->GetAttribute()->Set(
      std::string(kTestReplacementFileContents)));
  // Check that the file has immediately been updated.
  std::string output_value;
  ASSERT_OK(system_interface.ReadFileToString(kTestFilepath, &output_value));
  EXPECT_EQ(output_value, kTestReplacementFileContents);
  // The datasource should pick this change up without a cache update.
  EXPECT_THAT(datasource->GetAttribute(),
              ContainsValue<std::string>(kTestReplacementFileContents));
  // And the value should remain after another cache update.
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(datasource->GetAttribute(),
              ContainsValue<std::string>(kTestReplacementFileContents));
}

TEST(ReaderWriterDataSourceTest, CanReadIntegerFromFile) {
  SystemFake system_interface;
  system_interface.AddFakeFile(kTestFilepath, "123");

  auto datasource = ReaderWriterDataSource<int32>::Make(
      absl::make_unique<FilepathStringSource>(&system_interface, kTestFilepath),
      new NoCache());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(datasource->GetAttribute(), ContainsValue<int32>(123));
}

TEST(ReaderWriterDataSourceTest, IntegerReadFailsOnInvalidFile) {
  SystemFake system_interface;
  system_interface.AddFakeFile(kTestFilepath, "123 invalid");

  auto datasource = ReaderWriterDataSource<int32>::Make(
      absl::make_unique<FilepathStringSource>(&system_interface, kTestFilepath),
      new NoCache());
  EXPECT_THAT(datasource->UpdateValues(),
              StatusIs(_, _, HasSubstr("Failed to parse requested type")));
}

TEST(ReaderWriterDataSourceTest, CanReadDoubleFromFile) {
  SystemFake system_interface;
  system_interface.AddFakeFile(kTestFilepath, "-123.456");

  auto datasource = ReaderWriterDataSource<double>::Make(
      absl::make_unique<FilepathStringSource>(&system_interface, kTestFilepath),
      new NoCache());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(datasource->GetAttribute(), ContainsValue<double>(-123.456));
}

TEST(ReaderWriterDataSourceTest, CanUseIntegerFileWithModifierFunctions) {
  SystemFake system_interface;
  system_interface.AddFakeFile(kTestFilepath, "10");

  auto datasource = ReaderWriterDataSource<int32>::Make(
      absl::make_unique<FilepathStringSource>(&system_interface, kTestFilepath,
                                              true),
      new NoCache());
  datasource->AddModifierFunctions([](int32 in) -> int32 { return in * 2; },
                                   [](int32 out) -> int32 { return out / 2; });
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(datasource->GetAttribute(), ContainsValue<int32>(20));
  EXPECT_OK(datasource->GetAttribute()->Set(30));
  // The attribute value should not be passed through the modifier function.
  EXPECT_THAT(datasource->GetAttribute(), ContainsValue<int32>(30));
  // But the value written to the output file *should* be passed through.
  std::string output_value;
  ASSERT_OK(system_interface.ReadFileToString(kTestFilepath, &output_value));
  EXPECT_EQ(output_value, "15");
}

TEST(ReaderWriterDataSourceTest, CanUseModifierFunctionsForValidation) {
  SystemFake system_interface;
  system_interface.AddFakeFile(kTestFilepath, "12345");

  auto datasource = ReaderWriterDataSource<int32>::Make(
      absl::make_unique<FilepathStringSource>(&system_interface, kTestFilepath,
                                              true),
      new NoCache());
  datasource->AddModifierFunctions(
      [](int32 in) -> ::util::StatusOr<int32> {
        return MAKE_ERROR() << "Input failed validation.";
      },
      [](int32 out) -> ::util::StatusOr<int32> {
        return MAKE_ERROR() << "Output failed validation.";
      });
  EXPECT_THAT(datasource->UpdateValues(),
              StatusIs(_, _, HasSubstr("Input failed validation.")));
  EXPECT_THAT(datasource->GetAttribute()->Set(54321),
              StatusIs(_, _, HasSubstr("Output failed validation.")));
}

}  // namespace
}  // namespace phal
}  // namespace hal
}  // namespace stratum
