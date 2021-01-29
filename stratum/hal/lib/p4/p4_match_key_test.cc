// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This file contains unit tests for P4MatchKey and its subclasses.

#include "stratum/hal/lib/p4/p4_match_key.h"

#include <string>
#include <tuple>

#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/public/lib/error.h"

DECLARE_bool(enforce_bytestring_length);

using ::testing::Combine;
using ::testing::HasSubstr;
using ::testing::Range;

namespace stratum {
namespace hal {

// This class is the P4MatchKey test fixture.
class P4MatchKeyTest : public testing::Test {
 protected:
  // Various methods populate test_match_ with different types of field
  // matches for tests.
  void SetUpExactMatch(const std::string& exact_value) {
    test_match_.set_field_id(1);  // The ID is a don't care for all tests.
    test_match_.mutable_exact()->set_value(exact_value);
  }
  void SetUpLPMMatch(const std::string& lpm_value, int32_t prefix_length) {
    test_match_.set_field_id(1);  // The ID is a don't care for all tests.
    test_match_.mutable_lpm()->set_value(lpm_value);
    test_match_.mutable_lpm()->set_prefix_len(prefix_length);
  }
  void SetUpTernaryMatch(const std::string& ternary_value,
                         const std::string& ternary_mask) {
    test_match_.set_field_id(1);  // The ID is a don't care for all tests.
    test_match_.mutable_ternary()->set_value(ternary_value);
    test_match_.mutable_ternary()->set_mask(ternary_mask);
  }
  void SetUpRangeMatch(const std::string& low, const std::string& high) {
    test_match_.set_field_id(1);  // The ID is a don't care for all tests.
    test_match_.mutable_range()->set_low(low);
    test_match_.mutable_range()->set_high(high);
  }

  // P4 runtime FieldMatch that tests to use to create P4MatchKey instances.
  ::p4::v1::FieldMatch test_match_;

  // Portion of P4 table map field descriptor specifying conversion attributes.
  P4FieldDescriptor::P4FieldConversionEntry field_conversion_;

  // Common place for tests to store P4MatchKey::Convert output.
  MappedField mapped_field_;
};

// P4MatchKeyBitWidthTest is a parameterized subclass of P4MatchKeyTest.
// See INSTANTIATE_TEST_CASE_P below for the parameter formats.
class P4MatchKeyBitWidthTest
    : public P4MatchKeyTest,
      public testing::WithParamInterface<std::tuple<int, int>> {
 protected:
  // Test parameter accessors.
  int tested_width_param() const { return ::testing::get<0>(GetParam()); }
  int max_width_param() const { return ::testing::get<1>(GetParam()); }
};

// P4MatchKeyLPMPrefixWidthTest is a parameterized subclass of P4MatchKeyTest.
// See INSTANTIATE_TEST_CASE_P below for the parameter formats.
class P4MatchKeyLPMPrefixWidthTest
    : public P4MatchKeyTest,
      public testing::WithParamInterface<std::tuple<int, int>> {
 protected:
  // Test parameter accessors.
  int prefix_length_param() const { return ::testing::get<0>(GetParam()); }
  int lpm_field_width_param() const { return ::testing::get<1>(GetParam()); }
};

TEST_F(P4MatchKeyTest, TestCreateExactMatch) {
  const std::string kExactValue = {1, 2, 3};
  SetUpExactMatch(kExactValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  EXPECT_EQ(::p4::config::v1::MatchField::EXACT,
            match_key->allowed_match_type());
}

TEST_F(P4MatchKeyTest, TestCreateLPMMatch) {
  const std::string kLpmValue = {192, 168, 1, 1};
  SetUpLPMMatch(kLpmValue, 24);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  EXPECT_EQ(::p4::config::v1::MatchField::LPM, match_key->allowed_match_type());
}

TEST_F(P4MatchKeyTest, TestCreateTernaryMatch) {
  const std::string kTernaryValue = {4, 3, 2, 1, 0};
  const std::string kTernaryMask = {0xff, 0x00, 0xff, 0x00, 0xff};
  SetUpTernaryMatch(kTernaryValue, kTernaryMask);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  EXPECT_EQ(::p4::config::v1::MatchField::TERNARY,
            match_key->allowed_match_type());
}

TEST_F(P4MatchKeyTest, TestCreateRangeMatch) {
  const std::string kLowValue = {1, 2};
  const std::string kHighValue = {3, 4};
  SetUpRangeMatch(kLowValue, kHighValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  EXPECT_EQ(::p4::config::v1::MatchField::RANGE,
            match_key->allowed_match_type());
}

TEST_F(P4MatchKeyTest, TestCreateUnspecifiedMatch) {
  test_match_.set_field_id(1);  // The ID is a don't care for all tests.
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  EXPECT_EQ(::p4::config::v1::MatchField::UNSPECIFIED,
            match_key->allowed_match_type());
}

// Tests behavior for a match type conflict.  In this test, a match that the
// P4 table map identifies as TERNARY is encoded as EXACT in the P4 runtime
// request.
TEST_F(P4MatchKeyTest, TestMatchTypeConflict) {
  const std::string kExactValue = {4, 3};
  SetUpExactMatch(kExactValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::TERNARY);
  ::util::Status status =
      match_key->Convert(field_conversion_, 0, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("cannot convert to"));
}

// Tests exact match conversion to unsigned 32-bit integer.
TEST_F(P4MatchKeyTest, TestConvertExactMatch32) {
  const std::string kExactValue = {1, 2, 3, 4};
  const uint32_t kExpectedValue = 0x01020304;
  SetUpExactMatch(kExactValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, 31, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_FALSE(mapped_field_.has_mask());
  EXPECT_EQ(kExpectedValue, mapped_field_.value().u32());
}

// Tests exact match conversion to unsigned 64-bit integer.
TEST_F(P4MatchKeyTest, TestConvertExactMatch64) {
  const std::string kExactValue = {8, 7, 6, 5, 4, 3, 2, 1};
  const uint64_t kExpectedValue = 0x0807060504030201;
  SetUpExactMatch(kExactValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U64);
  ::util::Status status =
      match_key->Convert(field_conversion_, 63, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_FALSE(mapped_field_.has_mask());
  EXPECT_EQ(kExpectedValue, mapped_field_.value().u64());
}

// Tests LPM match conversion to unsigned 32-bit value and mask.
TEST_F(P4MatchKeyTest, TestConvertLPMMatch32) {
  const std::string kLPMValue = {192, 168, 1, 0};
  const uint32_t kExpectedValue = 0xc0a80100;
  const int kPrefixLength = 24;
  SetUpLPMMatch(kLPMValue, kPrefixLength);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_TRUE(mapped_field_.has_mask());
  EXPECT_EQ(kExpectedValue, mapped_field_.value().u32());
  EXPECT_EQ(static_cast<uint32_t>((1 << kPrefixLength) - 1) << 8,
            mapped_field_.mask().u32());
}

// Tests LPM match conversion to unsigned 64-bit value and mask.
TEST_F(P4MatchKeyTest, TestConvertLPMMatch64) {
  const std::string kLPMValue = {0xab, 0xcd, 0xef, 0, 1, 2, 3, 4};
  const uint64_t kExpectedValue = 0xabcdef0001020304ULL;
  const int kPrefixLength = 48;
  SetUpLPMMatch(kLPMValue, kPrefixLength);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U64_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 64, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_TRUE(mapped_field_.has_mask());
  EXPECT_EQ(kExpectedValue, mapped_field_.value().u64());
  EXPECT_EQ(0xffffffffffff0000ULL, mapped_field_.mask().u64());
}

// Tests LPM match conversion to byte-string-encoded value and mask.
TEST_F(P4MatchKeyTest, TestConvertLPMMatchBytes) {
  const std::string kLPMValue = {0xab, 0xcd, 0xef, 0,    1, 2, 3, 4,
                                 0xfe, 0xdc, 0xba, 0x80, 0, 0, 0, 0};
  const int kPrefixLength = 100;
  SetUpLPMMatch(kLPMValue, kPrefixLength);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_BYTES_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 128, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_TRUE(mapped_field_.has_mask());
  EXPECT_EQ(kLPMValue, mapped_field_.value().b());
  const std::string kExpectedMask = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xf0, 0,    0,    0};
  EXPECT_EQ(kExpectedMask, mapped_field_.mask().b());
}

// Tests ternary match conversion to unsigned 32-bit value and mask.
TEST_F(P4MatchKeyTest, TestConvertTernaryMatch32) {
  const std::string kTernaryValue = {192, 168, 1, 0};
  const uint32_t kExpectedValue = 0xc0a80100;
  const std::string kTernaryMask = {0xff, 0x00, 0xff, 0x00};
  const uint32_t kExpectedMask = 0xff00ff00;
  SetUpTernaryMatch(kTernaryValue, kTernaryMask);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::TERNARY);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_TRUE(mapped_field_.has_mask());
  EXPECT_EQ(kExpectedValue, mapped_field_.value().u32());
  EXPECT_EQ(kExpectedMask, mapped_field_.mask().u32());
}

// Tests ternary match conversion to unsigned 64-bit value and mask.
TEST_F(P4MatchKeyTest, TestConvertTernaryMatch64) {
  const std::string kTernaryValue = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0, 0};
  const uint64_t kExpectedValue = 0xffeeddccbbaa0000ULL;
  const std::string kTernaryMask = {0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf};
  const uint64_t kExpectedMask = 0x0f0f0f0f0f0f0f0f;
  SetUpTernaryMatch(kTernaryValue, kTernaryMask);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::TERNARY);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U64_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 64, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_TRUE(mapped_field_.has_mask());
  EXPECT_EQ(kExpectedValue, mapped_field_.value().u64());
  EXPECT_EQ(kExpectedMask, mapped_field_.mask().u64());
}

// Tests ternary match conversion to byte-string-encoded value and mask.
TEST_F(P4MatchKeyTest, TestConvertTernaryMatchBytes) {
  const std::string kTernaryValue = {0x0b, 0xcd};
  const std::string kTernaryMask = {0xf, 0xf0};
  SetUpTernaryMatch(kTernaryValue, kTernaryMask);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::TERNARY);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_BYTES_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 12, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_TRUE(mapped_field_.has_mask());
  EXPECT_EQ(kTernaryValue, mapped_field_.value().b());
  EXPECT_EQ(kTernaryMask, mapped_field_.mask().b());
}

// Tests conversion that the field descriptor data doesn't know how to convert.
TEST_F(P4MatchKeyTest, TestConvertUnknown) {
  const std::string kExactValue = {8, 7};
  SetUpExactMatch(kExactValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_UNKNOWN);
  ::util::Status status =
      match_key->Convert(field_conversion_, 16, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("unknown value conversion"));
  EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
}

// Tests conversion when the field descriptor specifies a raw match field copy.
TEST_F(P4MatchKeyTest, TestConvertRaw) {
  const std::string kExactValue = {8, 7};
  SetUpExactMatch(kExactValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_RAW);
  ::util::Status status =
      match_key->Convert(field_conversion_, 16, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
}

// Tests conversion of a match against a boolean "valid" key.
// TODO(teverman): This is currently just a raw conversion until support for
// this key type is implemented.
// FIXME(boc) google only
/*
TEST_F(P4MatchKeyTest, TestConvertValidMatch) {
  SetUpValidMatch(true);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(p4::config::MatchField::VALID);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_RAW);
  ::util::Status status =
      match_key->Convert(field_conversion_, 1, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
}
 */

// Tests conversion of a match against a range key.
// TODO(unknown): This is currently just a raw conversion until support for
// this key type is implemented.
TEST_F(P4MatchKeyTest, TestConvertRangeMatch) {
  const std::string kLowValue = {1, 2};
  const std::string kHighValue = {3, 4};
  SetUpRangeMatch(kLowValue, kHighValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::RANGE);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_RAW);
  ::util::Status status =
      match_key->Convert(field_conversion_, 1, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(mapped_field_.has_value());
  EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
}

// Tests conversion when the match request does not specify any data in
// the FieldMatch::field_match_type for an LPM match type.
TEST_F(P4MatchKeyTest, TestConvertDefaultLPMMatch) {
  test_match_.set_field_id(1);  // The ID is a don't care for all tests.
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  EXPECT_EQ(::p4::config::v1::MatchField::UNSPECIFIED,
            match_key->allowed_match_type());
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(mapped_field_.has_value());
}

// Tests conversion when the match request does not specify any data in
// the FieldMatch::field_match_type for a TERNARY match type.
TEST_F(P4MatchKeyTest, TestConvertDefaultTernaryMatch) {
  test_match_.set_field_id(1);  // The ID is a don't care for all tests.
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  EXPECT_EQ(::p4::config::v1::MatchField::UNSPECIFIED,
            match_key->allowed_match_type());
  field_conversion_.set_match_type(::p4::config::v1::MatchField::TERNARY);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(mapped_field_.has_value());
}

// Tests conversion when the match request does not specify any data in
// the FieldMatch::field_match_type for a RANGE match type.
TEST_F(P4MatchKeyTest, TestConvertDefaultRangeMatch) {
  test_match_.set_field_id(1);  // The ID is a don't care for all tests.
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  EXPECT_EQ(::p4::config::v1::MatchField::UNSPECIFIED,
            match_key->allowed_match_type());
  field_conversion_.set_match_type(::p4::config::v1::MatchField::RANGE);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(mapped_field_.has_value());
}

// Tests conversion when the match request does not specify any data in
// the FieldMatch::field_match_type for an EXACT match type.
TEST_F(P4MatchKeyTest, TestConvertUnspecifiedExactMatch) {
  test_match_.set_field_id(1);  // The ID is a don't care for all tests.
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  EXPECT_EQ(::p4::config::v1::MatchField::UNSPECIFIED,
            match_key->allowed_match_type());
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("EXACT"));
  EXPECT_THAT(status.ToString(), HasSubstr("has no default value"));
}

// Tests mismatch between a field's bitwidth and the specified conversion type.
// The most likely cause of this error is invalid table map data. For example,
// the table map says to convert a field with width > 32 to a 32 bit value.
TEST_F(P4MatchKeyTest, TestInvalidBitWidth) {
  const std::string kExactValue = {1, 2, 3, 4};
  SetUpExactMatch(kExactValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, 33, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("exceeds maximum unsigned width"));
  EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
}

// Tests conversion when an LPM match request encounters field descriptor data
// that does not specify any mask output for the prefix length.
TEST_F(P4MatchKeyTest, TestConvertLPMNoMaskConversion) {
  const std::string kLPMValue = {192, 168, 1, 0};
  const int kPrefixLength = 24;
  SetUpLPMMatch(kLPMValue, kPrefixLength);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_OPER_NOT_SUPPORTED, status.error_code());
  EXPECT_THAT(status.ToString(),
              HasSubstr("does not specify how to convert prefix"));
  EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
}

// Tests conversion when a ternary match request encounters field descriptor
// data that does not specify any mask output.
TEST_F(P4MatchKeyTest, TestConvertTernaryNoMaskConversion) {
  const std::string kTernaryValue = {0x08, 0xff};
  const std::string kTernaryMask = {0xf, 0xf0};
  SetUpTernaryMatch(kTernaryValue, kTernaryMask);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::TERNARY);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, 12, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_OPER_NOT_SUPPORTED, status.error_code());
  EXPECT_THAT(status.ToString(),
              HasSubstr("does not specify how to convert ternary mask"));
  EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
}

// Tests exact match conversion with an empty byte string value.
TEST_F(P4MatchKeyTest, TestExactMatchEmptyValue) {
  SetUpExactMatch({});
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, 31, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("Exact match field has no value"));
}

// Tests LPM match conversion with an empty byte string value.
TEST_F(P4MatchKeyTest, TestLPMMatchEmptyValue) {
  SetUpLPMMatch({}, 24);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(),
              HasSubstr("LPM match field is missing value or prefix length"));
}

// Tests LPM match conversion with a zero prefix length.
TEST_F(P4MatchKeyTest, TestLPMMatchZeroPrefix) {
  SetUpLPMMatch({192, 168, 1, 0}, 0);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(),
              HasSubstr("LPM match field is missing value or prefix length"));
}

// Tests ternary match conversion with an empty byte string value.
TEST_F(P4MatchKeyTest, TestTernaryMatchEmptyValue) {
  SetUpTernaryMatch({}, {0xff, 0x00, 0xff, 0x00});
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::TERNARY);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(),
              HasSubstr("Ternary match field is missing value or mask"));
}

// Tests ternary match conversion with an empty byte string mask.
TEST_F(P4MatchKeyTest, TestTernaryMatchEmptyMask) {
  SetUpTernaryMatch({192, 168, 1, 0}, {});
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::TERNARY);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, 32, &mapped_field_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(),
              HasSubstr("Ternary match field is missing value or mask"));
}

// Tests exact match conversion to unsigned 64-bit integer via
// ConvertExactToUint64.
TEST_F(P4MatchKeyTest, TestConvertExactToUint64) {
  const std::string kExactValue = {8, 7, 6, 5, 4, 3, 2, 1};
  const uint64 kExpectedValue = 0x0807060504030201;
  SetUpExactMatch(kExactValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  ::util::StatusOr<uint64> status = match_key->ConvertExactToUint64();
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(kExpectedValue, status.ValueOrDie());
}

// Tests exact match conversion to unsigned 64-bit integer via
// ConvertExactToUint64 when the input string exceeds 64 bits.
TEST_F(P4MatchKeyTest, TestConvertExactToUint64TooWide) {
  const std::string kExactValue = {9, 8, 7, 6, 5, 4, 3, 2, 1};
  SetUpExactMatch(kExactValue);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  ::util::StatusOr<uint64> status = match_key->ConvertExactToUint64();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.status().error_code());
}

// Tests exact match conversion to unsigned 64-bit integer via
// ConvertExactToUint64 when the match field is LPM.
TEST_F(P4MatchKeyTest, TestConvertExactToUint64LPM) {
  SetUpLPMMatch({192, 168, 1, 0}, 0);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  ::util::StatusOr<uint64> status = match_key->ConvertExactToUint64();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.status().error_code());
}

// Tests exact match conversion to unsigned 64-bit integer via
// ConvertExactToUint64 when the match field is ternary.
TEST_F(P4MatchKeyTest, TestConvertExactToUint64Ternary) {
  SetUpTernaryMatch({192, 168, 1, 0}, {});
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  ::util::StatusOr<uint64> status = match_key->ConvertExactToUint64();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.status().error_code());
}

// Tests exact match conversion to unsigned 64-bit integer via
// ConvertExactToUint64 when the match field is a range.
TEST_F(P4MatchKeyTest, TestConvertExactToUint64Range) {
  SetUpRangeMatch({1, 2}, {3, 4});
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  ::util::StatusOr<uint64> status = match_key->ConvertExactToUint64();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.status().error_code());
}

// Tests various combinations of exact match field bit width from 1 to 8.
// The test parameter defines two width attributes:
//  1) tested_width_param() is the field value width in the match request.
//  2) max_width_param() is the P4 program's width limit for the match field.
TEST_P(P4MatchKeyBitWidthTest, TestExactMatchBitWidths1to8) {
  const uint8_t expected_value = (1 << (tested_width_param() - 1)) | 1;
  const std::string match_value = {expected_value};
  SetUpExactMatch(match_value);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, max_width_param(), &mapped_field_);

  // Conversion succeeds or fails based on whether the tested bit width in the
  // request can fit within the maximum width of the output.
  bool expected_ok = (tested_width_param() <= max_width_param());
  EXPECT_EQ(expected_ok, status.ok());
  if (expected_ok) {
    EXPECT_EQ(expected_value, mapped_field_.value().u32());
  } else {
    EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
    EXPECT_THAT(status.ToString(),
                HasSubstr("exceeds the P4Runtime-defined width"));
    EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
  }
}

// Tests various combinations of exact match field bit width from 25 to 32.
// The test parameter is the same as TestExactMatchBitWidths1to8, but the tested
// field values are adjusted to use the high byte instead of the low byte.
TEST_P(P4MatchKeyBitWidthTest, TestExactMatchBitWidths25to32) {
  const uint8_t expected_high_byte = (1 << (tested_width_param() - 1)) | 1;
  const uint32_t expected_value = (expected_high_byte << 24) + 0x020304;
  const std::string match_value = {expected_high_byte, 2, 3, 4};
  SetUpExactMatch(match_value);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status = match_key->Convert(
      field_conversion_, max_width_param() + 24, &mapped_field_);

  // Conversion succeeds or fails based on whether the tested bit width in the
  // request can fit within the maximum width of the output.
  bool expected_ok = (tested_width_param() <= max_width_param());
  EXPECT_EQ(expected_ok, status.ok());
  if (expected_ok) {
    EXPECT_EQ(expected_value, mapped_field_.value().u32());
  } else {
    EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
    EXPECT_THAT(status.ToString(),
                HasSubstr("exceeds the P4Runtime-defined width"));
    EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
  }
}

// Tests various combinations of exact match field bit width from 57 to 64.
// The test parameter is the same as TestExactMatchBitWidths1to8, but the tested
// field values are adjusted to use the high byte of a uint64.
TEST_P(P4MatchKeyBitWidthTest, TestExactMatchBitWidths57to64) {
  const uint8_t expected_high_byte = (1 << (tested_width_param() - 1)) | 1;
  uint64_t expected_value = expected_high_byte;
  expected_value = (expected_value << 56) + 0x07060504030201ULL;
  const std::string match_value = {expected_high_byte, 7, 6, 5, 4, 3, 2, 1};
  SetUpExactMatch(match_value);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U64);
  ::util::Status status = match_key->Convert(
      field_conversion_, max_width_param() + 56, &mapped_field_);

  // Conversion succeeds or fails based on whether the tested bit width in the
  // request can fit within the maximum width of the output.
  bool expected_ok = (tested_width_param() <= max_width_param());
  EXPECT_EQ(expected_ok, status.ok());
  if (expected_ok) {
    EXPECT_EQ(expected_value, mapped_field_.value().u64());
  } else {
    EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
    EXPECT_THAT(status.ToString(),
                HasSubstr("exceeds the P4Runtime-defined width"));
    EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
  }
}

// Tests exact match field bit width combinations with extra leading zeroes,
// enforcing the P4Runtime bytestring length restrictions.  This test must
// run with FLAGS_enforce_bytestring_length enabled.
TEST_P(P4MatchKeyBitWidthTest, TestExactMatchBitEnforceExtraLeadingZeroes) {
  FLAGS_enforce_bytestring_length = true;
  const uint8_t expected_value = (1 << (tested_width_param() - 1)) | 1;
  const std::string match_value = {0, 0, expected_value};  // Two extra zeroes.
  SetUpExactMatch(match_value);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, max_width_param(), &mapped_field_);

  // Conversion always fails due to the extra leading zeroes in the field.
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(),
              HasSubstr("conform to P4Runtime-defined width"));
  EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
}

// Tests exact match field bit width combinations with extra leading zeroes.
// relaxing the P4Runtime bytestring length restrictions.  This test must
// run with FLAGS_enforce_bytestring_length disabled.
TEST_P(P4MatchKeyBitWidthTest, TestExactMatchBitExtraLeadingZeroesOK) {
  FLAGS_enforce_bytestring_length = false;
  const uint8_t expected_value = (1 << (tested_width_param() - 1)) | 1;
  const std::string match_value = {0, 0, expected_value};  // Two extra zeroes.
  SetUpExactMatch(match_value);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);
  ::util::Status status =
      match_key->Convert(field_conversion_, max_width_param(), &mapped_field_);

  // Conversion succeeds or fails based on whether the tested bit width in the
  // request can fit within the maximum width of the output.
  bool expected_ok = (tested_width_param() <= max_width_param());
  EXPECT_EQ(expected_ok, status.ok());
  if (expected_ok) {
    EXPECT_EQ(expected_value, mapped_field_.value().u32());
  } else {
    EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
    EXPECT_THAT(status.ToString(),
                HasSubstr("exceeds the P4Runtime-defined width"));
    EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
  }
}

// Tests exact match field bit width combinations missing leading zeroes,
// enforcing the P4Runtime bytestring length restrictions.  This test must
// run with FLAGS_enforce_bytestring_length enabled.
TEST_P(P4MatchKeyBitWidthTest, TestExactMatchBitEnforceMissingLeadingZeroes) {
  FLAGS_enforce_bytestring_length = true;
  const uint8_t expected_value = (1 << (tested_width_param() - 1)) | 1;
  const std::string match_value = {expected_value};  // Value is only one byte.
  SetUpExactMatch(match_value);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);

  // The P4 bit width is adjustment by 8 bits to require a two-byte value.
  ::util::Status status = match_key->Convert(
      field_conversion_, max_width_param() + 8, &mapped_field_);

  // Conversion always fails due to the missing leading zeroes in the field.
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.ToString(),
              HasSubstr("conform to P4Runtime-defined width"));
  EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
}

// Tests exact match field bit width combinations missing leading zeroes,
// relaxing the P4Runtime bytestring length restrictions.  This test must
// run with FLAGS_enforce_bytestring_length disabled.
TEST_P(P4MatchKeyBitWidthTest, TestExactMatchBitMissingLeadingZeroesOK) {
  FLAGS_enforce_bytestring_length = false;
  const uint8_t expected_value = (1 << (tested_width_param() - 1)) | 1;
  const std::string match_value = {expected_value};  // Value is only one byte.
  SetUpExactMatch(match_value);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::EXACT);
  field_conversion_.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U32);

  // The P4 bit width is adjustment by 8 bits to require a two-byte value.
  ::util::Status status = match_key->Convert(
      field_conversion_, max_width_param() + 8, &mapped_field_);

  // Conversion always succeeds since P4MatchKey doesn't require leading zeroes.
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(expected_value, mapped_field_.value().u32());
}

// Tests LPM match bit width and prefix length combinations for uint32 value
// and mask output.  The attributes in the test parameter are:
//  1) lpm_field_width_param() - the width of the encoded value byte string.
//  2) prefix_length_param() - the length of the prefix in the match request.
TEST_P(P4MatchKeyLPMPrefixWidthTest, TestLPMPrefixLength32) {
  const uint8_t expected_high_byte =
      (1 << ((lpm_field_width_param() - 1) % 8)) | 1;
  uint32_t expected_value = expected_high_byte;
  std::string lpm_match_value = {expected_high_byte};
  for (int width = 8; width < lpm_field_width_param(); width += 8) {
    lpm_match_value.push_back(width);
    expected_value <<= 8;
    expected_value |= width;
  }
  SetUpLPMMatch(lpm_match_value, prefix_length_param());
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK);
  ::util::Status status = match_key->Convert(
      field_conversion_, lpm_field_width_param(), &mapped_field_);

  // Conversion succeeds or fails based on whether the tested prefix length
  // in the request exceeds the maximum width of the output.
  bool expected_ok = (prefix_length_param() <= lpm_field_width_param());
  EXPECT_EQ(expected_ok, status.ok());
  if (expected_ok) {
    EXPECT_EQ(expected_value, mapped_field_.value().u32());
    EXPECT_NE(0, mapped_field_.mask().u32());
    EXPECT_EQ(32 - lpm_field_width_param(),
              __builtin_clz(mapped_field_.mask().u32()));
    EXPECT_EQ(lpm_field_width_param() - prefix_length_param(),
              __builtin_ctz(mapped_field_.mask().u32()));
  } else {
    EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
    EXPECT_THAT(status.ToString(), HasSubstr("LPM prefix length"));
    EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
  }
}

// Tests LPM match bit width and prefix length combinations for uint64 value
// and mask output.  The test uses the same parameters as TestLPMPrefixLength32,
// adjusting them for the high-order 32 bits of the 64-bit match.
TEST_P(P4MatchKeyLPMPrefixWidthTest, TestLPMPrefixLength64) {
  const uint8_t expected_high_byte = (1 << (lpm_field_width_param() - 1)) | 1;
  uint64_t expected_value = expected_high_byte;
  expected_value <<= 32;
  expected_value |= 0xabcdef01;
  std::string lpm_match_value = {expected_high_byte, 0xab, 0xcd, 0xef, 0x01};
  int lpm_field_width = lpm_field_width_param() + 32;
  int prefix_length = prefix_length_param() + 32;
  for (int width = 40; width < lpm_field_width; width += 8) {
    lpm_match_value.push_back(width);
    expected_value <<= 8;
    expected_value |= width;
  }
  SetUpLPMMatch(lpm_match_value, prefix_length);
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_U64_AND_MASK);
  ::util::Status status =
      match_key->Convert(field_conversion_, lpm_field_width, &mapped_field_);

  // Conversion succeeds or fails based on whether the tested prefix length
  // in the request exceeds the maximum width of the output.
  bool expected_ok = (prefix_length <= lpm_field_width);
  EXPECT_EQ(expected_ok, status.ok());
  if (expected_ok) {
    EXPECT_EQ(expected_value, mapped_field_.value().u64());
    EXPECT_NE(0, mapped_field_.mask().u64());
    EXPECT_EQ(64 - lpm_field_width,
              __builtin_clzll(mapped_field_.mask().u64()));
    EXPECT_EQ(lpm_field_width - prefix_length,
              __builtin_ctzll(mapped_field_.mask().u64()));
  } else {
    EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
    EXPECT_THAT(status.ToString(), HasSubstr("LPM prefix length"));
    EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
  }
}

// Tests LPM match bit width and prefix length combinations for value
// and mask output to a binary-encoded byte string.
TEST_P(P4MatchKeyLPMPrefixWidthTest, TestLPMPrefixLengthBytes) {
  const uint8_t expected_high_byte =
      (1 << ((lpm_field_width_param() - 1) % 8)) | 1;
  std::string lpm_match_value = {expected_high_byte};
  for (int width = 8; width < lpm_field_width_param(); width += 8) {
    lpm_match_value.push_back(width);
  }
  SetUpLPMMatch(lpm_match_value, prefix_length_param());
  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(test_match_);
  field_conversion_.set_match_type(::p4::config::v1::MatchField::LPM);
  field_conversion_.set_conversion(
      P4FieldDescriptor::P4_CONVERT_TO_BYTES_AND_MASK);
  ::util::Status status = match_key->Convert(
      field_conversion_, lpm_field_width_param(), &mapped_field_);
  bool expected_ok = (prefix_length_param() <= lpm_field_width_param());
  EXPECT_EQ(expected_ok, status.ok());

  // Conversion succeeds or fails based on whether the tested prefix length
  // in the request exceeds the maximum width of the output.
  if (expected_ok) {
    EXPECT_EQ(lpm_match_value, mapped_field_.value().b());
    const int expected_mask_width = 1 + (lpm_field_width_param() - 1) / 8;
    EXPECT_EQ(expected_mask_width, mapped_field_.mask().b().size());
    // TODO(unknown): Add more detailed expectations for the mask value.
  } else {
    EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
    EXPECT_THAT(status.ToString(), HasSubstr("LPM prefix length"));
    EXPECT_TRUE(mapped_field_.value().has_raw_pi_match());
  }
}

// This set of test parameters enumerates all combinations of match field
// request bit width vs. the bit length of the field in the P4 program.
INSTANTIATE_TEST_CASE_P(BitWidthTestParameters, P4MatchKeyBitWidthTest,
                        Combine(Range(1, 9),    // 1 to 8 inclusive.
                                Range(1, 9)));  // 1 to 8 inclusive.

// This set of test parameters enumerates all combinations of match field
// request LPM prefix length vs. the bit length of the field in the P4 program.
INSTANTIATE_TEST_CASE_P(LPMPrefixTestParameters, P4MatchKeyLPMPrefixWidthTest,
                        Combine(Range(1, 33),    // 1 to 32 inclusive.
                                Range(1, 33)));  // 1 to 32 inclusive.

}  // namespace hal
}  // namespace stratum
