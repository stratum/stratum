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


// Unit tests for p4_utils.

#include "stratum/hal/lib/p4/utils.h"

#include "PI/proto/util.h"
#include "stratum/hal/lib/p4/p4_runtime_mock.h"
#include "stratum/lib/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/substitute.h"

using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::_;
using pi::proto::util::P4ResourceType;

namespace stratum {
namespace hal {

// The test fixture provides a mock P4 API instance for these tests.
class PrintP4ObjectIDTest : public testing::Test {
 protected:
  // SetUp and TearDown replace the existing P4 runtime API instance, if any,
  // with a mock for these tests, then restore it when done.
  void SetUp() override {
    saved_api_ = P4RuntimeInterface::instance();
    P4RuntimeInterface::set_instance(&mock_api_);
  }
  void TearDown() override { P4RuntimeInterface::set_instance(saved_api_); }

  P4RuntimeMock mock_api_;
  P4RuntimeInterface* saved_api_;
};

namespace {

// P4 uses upper 8-bits of 32 bits to store the type.
int MakeP4ID(P4ResourceType resource_type, int base_id) {
  return (static_cast<int>(resource_type) << 24) + base_id;
}

}  // namespace

TEST_F(PrintP4ObjectIDTest, TestTableID) {
  const int kBaseID = 12345;
  const int test_p4_id = MakeP4ID(P4ResourceType::TABLE, kBaseID);
  EXPECT_CALL(mock_api_, GetResourceTypeFromID(_))
      .WillOnce(Return(P4ResourceType::TABLE));
  const std::string printed_id = PrintP4ObjectID(test_p4_id);
  EXPECT_FALSE(printed_id.empty());
  const std::string expected_print =
      absl::Substitute("TABLE/$0 ($1)", kBaseID, test_p4_id);
  EXPECT_THAT(printed_id, HasSubstr(expected_print));
}

TEST_F(PrintP4ObjectIDTest, TestActionID) {
  const int kBaseID = 54321;
  const int test_p4_id = MakeP4ID(P4ResourceType::ACTION, kBaseID);
  EXPECT_CALL(mock_api_, GetResourceTypeFromID(_))
      .WillOnce(Return(P4ResourceType::ACTION));
  const std::string printed_id = PrintP4ObjectID(test_p4_id);
  EXPECT_FALSE(printed_id.empty());
  const std::string expected_print =
      absl::Substitute("ACTION/$0 ($1)", kBaseID, test_p4_id);
  EXPECT_THAT(printed_id, HasSubstr(expected_print));
}

TEST_F(PrintP4ObjectIDTest, TestCounterID) {
  const int kBaseID = 0x13579b;
  const int test_p4_id = MakeP4ID(P4ResourceType::COUNTER, kBaseID);
  EXPECT_CALL(mock_api_, GetResourceTypeFromID(_))
      .WillOnce(Return(P4ResourceType::COUNTER));
  const std::string printed_id = PrintP4ObjectID(test_p4_id);
  EXPECT_FALSE(printed_id.empty());
  const std::string expected_print =
      absl::Substitute("COUNTER/$0 ($1)", kBaseID, test_p4_id);
  EXPECT_THAT(printed_id, HasSubstr(expected_print));
}

TEST_F(PrintP4ObjectIDTest, TestInvalidID) {
  const int test_p4_id = MakeP4ID(P4ResourceType::INVALID, 1);
  EXPECT_CALL(mock_api_, GetResourceTypeFromID(_))
      .WillOnce(Return(P4ResourceType::INVALID));
  const std::string printed_id = PrintP4ObjectID(test_p4_id);
  EXPECT_FALSE(printed_id.empty());
  EXPECT_THAT(printed_id, HasSubstr("INVALID/"));
}

}  // namespace hal
}  // namespace stratum
