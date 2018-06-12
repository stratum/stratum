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


#include "stratum/hal/lib/phal/fixed_layout_datasource.h"

#include <ctime>

#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/fixed_stringsource.h"
#include "stratum/hal/lib/phal/test/test.pb.h"
#include "stratum/hal/lib/phal/test_util.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace hal {
namespace phal {
namespace {

const std::string kTestBody{// NOLINT
                            0x01,       0x02, 0x03, 0x04, 0x01, 0x02,
                            0x00,       0xFF, 't',  'e',  's',  't',
                            0b11001100, 0x01, 0x02, 0x03};

TEST(FixedLayoutCreationTest, CanCreateLayoutWithAllFieldTypes) {
  std::map<std::string, FixedLayoutField*> fields{
      {"int32", new TypedField<int32>(0, 1)},
      {"int64", new TypedField<int64>(1, 1)},
      {"uint32", new TypedField<uint32>(2, 1)},
      {"uint64", new TypedField<uint64>(3, 1)},
      {"test_string", new TypedField<std::string>(4, 4)},
      {"bit", new BitmapBooleanField(8, 0)},
      {"enum", new EnumField(9, TopEnum_descriptor(), {{0, TopEnum::ZERO}})},
      {"valid", new ValidationByteField(10, {0x01}, "invalid!")},
      {"double", new FloatingField<double>(11, 1, true, 0.5)}};
  auto stringsource = absl::make_unique<FixedStringSource>(kTestBody);
  std::shared_ptr<FixedLayoutDataSource> datasource =
      FixedLayoutDataSource::Make(std::move(stringsource), fields,
                                  new NoCache());
  EXPECT_TRUE(datasource != nullptr);
}

TEST(FixedLayoutCreationTest, UpdateFailsIfBufferTooSmall) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"test_string", new TypedField<std::string>(4, 4)}};
  auto stringsource = absl::make_unique<FixedStringSource>("short");
  std::shared_ptr<FixedLayoutDataSource> datasource =
      FixedLayoutDataSource::Make(std::move(stringsource), fields,
                                  new NoCache());
  EXPECT_FALSE(datasource->UpdateValues().ok());
}

class FixedLayoutTest : public ::testing::Test {
 protected:
  void SetUp() override {
    stringsource_ = absl::make_unique<FixedStringSource>(kTestBody);

    // Set the time zone to UTC.
    tzname[0] = tzname[1] = const_cast<char*>("GMT");
    timezone = 0;
    daylight = 0;
    setenv("TZ", "UTC", 1);
  }

  std::unique_ptr<StringSourceInterface> stringsource_;
  std::shared_ptr<FixedLayoutDataSource> datasource_;
};

TEST_F(FixedLayoutTest, CanReadInt32Byte) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"int32", new TypedField<int32>(0, 1)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("int32"),
              IsOkAndContainsValue<int32>(1));
}

TEST_F(FixedLayoutTest, CanReadInt64Byte) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"int64", new TypedField<int64>(1, 1)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("int64"),
              IsOkAndContainsValue<int64>(2));
}

TEST_F(FixedLayoutTest, CanReadUInt32Byte) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"uint32", new TypedField<uint32>(2, 1)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("uint32"),
              IsOkAndContainsValue<uint32>(3));
}

TEST_F(FixedLayoutTest, CanReadUInt64Byte) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"uint64", new TypedField<uint64>(3, 1)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("uint64"),
              IsOkAndContainsValue<uint64>(4));
}

TEST_F(FixedLayoutTest, CanReadInt32MultiByte) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"int32", new TypedField<int32>(4, 4)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("int32"),
              IsOkAndContainsValue<int32>(0x010200FF));
}

TEST_F(FixedLayoutTest, CanReadLittleEndianInt32) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"int32", new TypedField<int32>(4, 4, true)},
      {"int24", new TypedField<int32>(4, 3, true)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("int32"),
              IsOkAndContainsValue<int32>(0xFF000201));
  EXPECT_THAT(datasource_->GetAttribute("int24"),
              IsOkAndContainsValue<int32>(0x201));
}

TEST_F(FixedLayoutTest, CanReadStringField) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"test_string", new TypedField<std::string>(8, 4)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("test_string"),
              IsOkAndContainsValue<std::string>("test"));
}

TEST_F(FixedLayoutTest, CanReadLittleEndianStringField) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"test_string", new TypedField<std::string>(8, 4, true)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("test_string"),
              IsOkAndContainsValue<std::string>("tset"));
}

TEST_F(FixedLayoutTest, CanReadFloatByte) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"float", new FloatingField<float>(0, 1, true, 1)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("float"),
              IsOkAndContainsValue<float>(1));
}

TEST_F(FixedLayoutTest, CanReadScaledMultibyteFloat) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"float", new FloatingField<float>(0, 2, true, 0.25)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  // 0x0102 = 258, and 258 / 4 = 64.5
  EXPECT_THAT(datasource_->GetAttribute("float"),
              IsOkAndContainsValue<float>(64.5));
}

TEST_F(FixedLayoutTest, CanReadScaledMultibyteDouble) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"double", new FloatingField<double>(0, 2, true, 0.25)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  // 0x0102 = 258, and 258 / 4 = 64.5
  EXPECT_THAT(datasource_->GetAttribute("double"),
              IsOkAndContainsValue<double>(64.5));
}

TEST_F(FixedLayoutTest, CanReadCleanedStringField) {
  std::string test_body{'t', 'e', 's', 't', 0x01, '!', ' ', ' '};
  auto stringsource = absl::make_unique<FixedStringSource>(test_body);
  std::map<std::string, FixedLayoutField*> fields = {
      {"test_string", new CleanedStringField(0, 8)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("test_string"),
              IsOkAndContainsValue<std::string>("test*!"));
}

TEST_F(FixedLayoutTest, CanReadUnsignedBitField) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"zero_bit", new UnsignedBitField(12, 5, 1)},   // bit 5 is 0
      {"one_bit", new UnsignedBitField(12, 6, 1)},    // bit 6 is 1
      {"multi_bit", new UnsignedBitField(12, 5, 2)},  // bits 6-5, i.e. 0x10
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("zero_bit"),
              IsOkAndContainsValue<uint32>(0));
  EXPECT_THAT(datasource_->GetAttribute("one_bit"),
              IsOkAndContainsValue<uint32>(1));
  EXPECT_THAT(datasource_->GetAttribute("multi_bit"),
              IsOkAndContainsValue<uint32>(2));
}

TEST_F(FixedLayoutTest, CanReadBitmapBooleanField) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"bit7", new BitmapBooleanField(12, 7)},
      {"bit6", new BitmapBooleanField(12, 6)},
      {"bit5", new BitmapBooleanField(12, 5)},
      {"bit4", new BitmapBooleanField(12, 4)},
      {"bit3", new BitmapBooleanField(12, 3)},
      {"bit2", new BitmapBooleanField(12, 2)},
      {"bit1", new BitmapBooleanField(12, 1)},
      {"bit0", new BitmapBooleanField(12, 0)},
  };
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  // Byte is 0b11001100.
  EXPECT_THAT(datasource_->GetAttribute("bit7"),
              IsOkAndContainsValue<bool>(true));
  EXPECT_THAT(datasource_->GetAttribute("bit6"),
              IsOkAndContainsValue<bool>(true));
  EXPECT_THAT(datasource_->GetAttribute("bit5"),
              IsOkAndContainsValue<bool>(false));
  EXPECT_THAT(datasource_->GetAttribute("bit4"),
              IsOkAndContainsValue<bool>(false));
  EXPECT_THAT(datasource_->GetAttribute("bit3"),
              IsOkAndContainsValue<bool>(true));
  EXPECT_THAT(datasource_->GetAttribute("bit2"),
              IsOkAndContainsValue<bool>(true));
  EXPECT_THAT(datasource_->GetAttribute("bit1"),
              IsOkAndContainsValue<bool>(false));
  EXPECT_THAT(datasource_->GetAttribute("bit0"),
              IsOkAndContainsValue<bool>(false));
}

TEST_F(FixedLayoutTest, CanCheckValidationByte) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"validation", new ValidationByteField(0, {0x01}, "should be valid")}};
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
}

TEST_F(FixedLayoutTest, CanCheckValidationByteWithMultiplePossible) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"validation", new ValidationByteField(2, {0x01, 0x02, 0x03, 0x04},
                                             "should be valid")}};
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
}

TEST_F(FixedLayoutTest, ValidationByteCanFail) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"validation",
       new ValidationByteField(0, {0x02, 0x03, 0x04}, "error expected!")}};
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_FALSE(datasource_->UpdateValues().ok());
}

TEST_F(FixedLayoutTest, MultipleValidationBytesCanFail) {
  std::map<std::string, FixedLayoutField*> fields = {
      {"validation1",
       new ValidationByteField(0, {0x01, 0x02, 0x03, 0x04}, "should be valid")},
      {"validation2",
       new ValidationByteField(1, {0x01, 0x02, 0x03, 0x04}, "should be valid")},
      {"validation3",
       new ValidationByteField(2, {0x01, 0x02, 0x04}, "error expected!")}};
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_FALSE(datasource_->UpdateValues().ok());
}

TEST_F(FixedLayoutTest, CanReadTimestampField) {
  std::string test_body = "100107";
  auto stringsource = absl::make_unique<FixedStringSource>(test_body);
  std::map<std::string, FixedLayoutField*> fields = {
      {"timestamp", new TimestampField(0, 6, "%y%m%d")}};
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource), fields,
                                            new NoCache());
  ASSERT_OK(datasource_->UpdateValues());
  EXPECT_THAT(datasource_->GetAttribute("timestamp"),
              IsOkAndContainsValue<uint32>(1262822400));
}

TEST_F(FixedLayoutTest, CannotReadInvalidTimestampField) {
  std::string test_body = "no timestamp here!";
  auto stringsource = absl::make_unique<FixedStringSource>(test_body);
  std::map<std::string, FixedLayoutField*> fields = {
      {"timestamp", new TimestampField(0, 6, "%y%m%d")}};
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource), fields,
                                            new NoCache());
  EXPECT_FALSE(datasource_->UpdateValues().ok());
}

TEST_F(FixedLayoutTest, CanReadEnumField) {
  std::map<char, int> enum_map{{0, TopEnum::ZERO},
                               {1, TopEnum::ONE},
                               {2, TopEnum::TWO},
                               {3, TopEnum::THREE}};
  std::map<std::string, FixedLayoutField*> fields = {
      {"enum1", new EnumField(13, TopEnum_descriptor(), enum_map)},
      {"enum2", new EnumField(14, TopEnum_descriptor(), enum_map)},
      {"enum3", new EnumField(15, TopEnum_descriptor(), enum_map)}};
  datasource_ = FixedLayoutDataSource::Make(std::move(stringsource_), fields,
                                            new NoCache());
  EXPECT_OK(datasource_->UpdateValues());
  EXPECT_THAT(
      datasource_->GetAttribute("enum1"),
      IsOkAndContainsValue(TopEnum_descriptor()->FindValueByName("ONE")));
  EXPECT_THAT(
      datasource_->GetAttribute("enum2"),
      IsOkAndContainsValue(TopEnum_descriptor()->FindValueByName("TWO")));
  EXPECT_THAT(
      datasource_->GetAttribute("enum3"),
      IsOkAndContainsValue(TopEnum_descriptor()->FindValueByName("THREE")));
}

TEST(SingleFixedLayoutTest, CannotReadInvalidEnum) {
  std::string test_body{0x01, 0x02, 0xAB};
  std::map<char, int> enum_map{{0, TopEnum::ZERO},
                               {1, TopEnum::ONE},
                               {2, TopEnum::TWO},
                               {3, TopEnum::THREE}};
  std::map<std::string, FixedLayoutField*> fields = {
      {"enum1", new EnumField(0, TopEnum_descriptor(), enum_map)},
      {"enum2", new EnumField(1, TopEnum_descriptor(), enum_map)},
      {"enum3", new EnumField(2, TopEnum_descriptor(), enum_map)}};
  auto stringsource = absl::make_unique<FixedStringSource>(test_body);
  std::shared_ptr<FixedLayoutDataSource> datasource =
      FixedLayoutDataSource::Make(std::move(stringsource), fields,
                                  new NoCache());
  EXPECT_FALSE(datasource->UpdateValues().ok());
}

TEST(SingleFixedLayoutTest, CanReadEnumFieldWithDefault) {
  std::string test_body{0x01};
  std::map<char, int> enum_map{
      {0, TopEnum::ZERO},
  };
  std::map<std::string, FixedLayoutField*> fields = {
      {"enum", new EnumField(0, TopEnum_descriptor(), enum_map, true,
                             TopEnum::TWO)}  // Set TWO as default.
  };
  auto stringsource = absl::make_unique<FixedStringSource>(test_body);
  std::shared_ptr<FixedLayoutDataSource> datasource =
      FixedLayoutDataSource::Make(std::move(stringsource), fields,
                                  new NoCache());
  EXPECT_OK(datasource->UpdateValues());
  EXPECT_THAT(
      datasource->GetAttribute("enum"),
      IsOkAndContainsValue(TopEnum_descriptor()->FindValueByName("TWO")));
}

}  // namespace
}  // namespace phal
}  // namespace hal
}  // namespace stratum
