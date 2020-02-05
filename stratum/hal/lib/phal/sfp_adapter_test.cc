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

#include "stratum/hal/lib/phal/sfp_adapter.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/attribute_database_mock.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DECLARE_int32(onlp_polling_interval_ms);

namespace stratum {
namespace hal {
namespace phal {

using TransceiverEvent = PhalInterface::TransceiverEvent;

using stratum::test_utils::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

static constexpr int kMaxXcvrEventDepth = 256;

class SfpAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = absl::make_unique<AttributeDatabaseMock>();
    sfp_adapter_ = absl::make_unique<SfpAdapter>(database_.get());
  }

  std::unique_ptr<AttributeDatabaseMock> database_;
  std::unique_ptr<SfpAdapter> sfp_adapter_;

  const std::string phaldb_get_response_proto = R"PROTO(
    cards {
      ports {
        physical_port_type: PHYSICAL_PORT_TYPE_SFP_CAGE
        transceiver {
          id: 0
          description: "port-0"
          hardware_state: HW_STATE_PRESENT
          media_type: MEDIA_TYPE_SFP
          connector_type: SFP_TYPE_SFP
          module_type: SFP_MODULE_TYPE_10G_BASE_CR
          info {
            mfg_name: "test_vendor"
            part_no: "test part #"
            serial_no: "test1234"
          }
        }
      }
    }
  )PROTO";
};

namespace {

TEST_F(SfpAdapterTest, OnlpPhalGetFrontPanelPortInfoSuccess) {
  auto db_query_mock = absl::make_unique<QueryMock>();
  auto db_query = db_query_mock.get();
  EXPECT_CALL(*database_.get(), MakeQuery(_))
      .WillOnce(Return(ByMove(
          ::util::StatusOr<std::unique_ptr<Query>>(std::move(db_query_mock)))));
  auto phaldb_resp = absl::make_unique<PhalDB>();
  ASSERT_OK(ParseProtoFromString(phaldb_get_response_proto, phaldb_resp.get()));
  EXPECT_CALL(*db_query, Get())
      .WillOnce(Return(ByMove(std::move(phaldb_resp))));
  FrontPanelPortInfo fp_port_info{};

  EXPECT_OK(sfp_adapter_->GetFrontPanelPortInfo(1, 1, &fp_port_info));
  EXPECT_EQ(fp_port_info.physical_port_type(), PHYSICAL_PORT_TYPE_SFP_CAGE);
  EXPECT_EQ(fp_port_info.media_type(), MEDIA_TYPE_SFP);
  EXPECT_EQ(fp_port_info.vendor_name(), "test_vendor");
  EXPECT_EQ(fp_port_info.part_number(), "test part #");
  EXPECT_EQ(fp_port_info.serial_number(), "test1234");
}

TEST_F(SfpAdapterTest, OnlpPhalGetFrontPanelPortInfoFailureInvalidPort) {
  auto db_query_mock = absl::make_unique<QueryMock>();
  auto db_query = db_query_mock.get();
  EXPECT_CALL(*database_.get(), MakeQuery(_))
      .WillOnce(Return(ByMove(
          ::util::StatusOr<std::unique_ptr<Query>>(std::move(db_query_mock)))));
  auto phaldb_resp = absl::make_unique<PhalDB>();
  ASSERT_OK(ParseProtoFromString(phaldb_get_response_proto, phaldb_resp.get()));
  EXPECT_CALL(*db_query, Get())
      .WillOnce(Return(ByMove(std::move(phaldb_resp))));
  FrontPanelPortInfo fp_port_info{};

  auto status = sfp_adapter_->GetFrontPanelPortInfo(1, 2, &fp_port_info);
  EXPECT_FALSE(status.ok()) << status;
}

// TODO(max): add tests
TEST_F(SfpAdapterTest, OnlpPhalRegisterAndUnregisterTransceiverEventWriter) {}

}  // namespace

}  // namespace phal
}  // namespace hal
}  // namespace stratum
