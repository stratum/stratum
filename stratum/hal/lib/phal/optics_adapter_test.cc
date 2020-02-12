// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

#include "stratum/hal/lib/phal/optics_adapter.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/attribute_database_mock.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace phal {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::ByMove;

class OpticsAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = absl::make_unique<AttributeDatabaseMock>();
    optics_adapter_ = absl::make_unique<OpticsAdapter>(database_.get());
  }

  std::unique_ptr<AttributeDatabaseMock> database_;
  std::unique_ptr<OpticsAdapter> optics_adapter_;

  const std::string phaldb_get_response_proto = R"PROTO(
    optical_cards {
      frequency: 196
      input_power: 1000.2
      output_power: 10000.1
      target_output_power: 15.5
      operational_mode: 1
    }
  )PROTO";
};

// Settable atrributes paths.
const std::vector<PathEntry> frequency_path
    = {PathEntry("optical_cards", 0), PathEntry("frequency")};
const std::vector<PathEntry> target_output_power_path
    = {PathEntry("optical_cards", 0), PathEntry("target_output_power")};
const std::vector<PathEntry> operational_mode_path
    = {PathEntry("optical_cards", 0), PathEntry("operational_mode")};

// Settable attributes matcher
MATCHER_P(DbAttributesEqual, other, "") {
  const bool equal_frequency = (
      absl::get<uint64>(arg.at(frequency_path))
      == absl::get<uint64>(other.at(frequency_path)));
  const bool equal_target_output_power = (
      absl::get<float>(arg.at(target_output_power_path))
      == absl::get<float>(other.at(target_output_power_path)));
  const bool equal_operational_mode = (
      absl::get<uint64>(arg.at(operational_mode_path))
      == absl::get<uint64>(other.at(operational_mode_path)));

  return equal_frequency && equal_target_output_power && equal_operational_mode;
};

TEST_F(OpticsAdapterTest, TaiPhalGetOpticalTransceiverInfoSuccess) {
  auto db_query_mock = absl::make_unique<QueryMock>();
  auto db_query = db_query_mock.get();
  EXPECT_CALL(*database_.get(), MakeQuery(_))
      .WillOnce(Return(ByMove(
          ::util::StatusOr<std::unique_ptr<Query>>(std::move(db_query_mock)))));

  auto phaldb_resp = absl::make_unique<PhalDB>();
  ASSERT_OK(ParseProtoFromString(phaldb_get_response_proto, phaldb_resp.get()));
  EXPECT_CALL(*db_query, Get())
      .WillOnce(Return(ByMove(std::move(phaldb_resp))));

  OpticalChannelInfo oc_info{};
  EXPECT_OK(optics_adapter_->GetOpticalTransceiverInfo(0, 0, &oc_info));

  EXPECT_EQ(oc_info.frequency().value(), 196);
  EXPECT_FLOAT_EQ(oc_info.input_power().instant(), 1000.2);
  EXPECT_FLOAT_EQ(oc_info.output_power().instant(), 10000.1);
  EXPECT_FLOAT_EQ(oc_info.target_output_power().value(), 15.5);
  EXPECT_EQ(oc_info.operational_mode().value(), 1);
}

TEST_F(OpticsAdapterTest, TaiPhalSetOpticalTransceiverInfoSuccess) {
  OpticalChannelInfo oc_info;
  oc_info.mutable_frequency()->set_value(150);
  oc_info.mutable_target_output_power()->set_value(140.12);
  oc_info.mutable_operational_mode()->set_value(3);

  AttributeValueMap attrs;
  attrs[frequency_path] = oc_info.frequency().value();
  attrs[target_output_power_path] = oc_info.target_output_power().value();
  attrs[operational_mode_path] = oc_info.operational_mode().value();

  EXPECT_CALL(*database_.get(), Set(DbAttributesEqual(attrs)))
      .WillOnce(Return(::util::Status::OK));
  EXPECT_OK(optics_adapter_->SetOpticalTransceiverInfo(0, 0, oc_info));
}

}  // namespace

}  // namespace phal
}  // namespace hal
}  // namespace stratum
