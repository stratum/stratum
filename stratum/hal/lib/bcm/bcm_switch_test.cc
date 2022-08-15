// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_switch.h"

#include <utility>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/canonical_errors.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_chassis_manager_mock.h"
#include "stratum/hal/lib/bcm/bcm_node_mock.h"
#include "stratum/hal/lib/bcm/bcm_packetio_manager_mock.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/hal/lib/p4/p4_table_mapper_mock.h"
#include "stratum/lib/channel/channel_mock.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace bcm {

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::WithArg;
using ::testing::WithArgs;

namespace {

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

MATCHER_P(EqualsStatus, status, "") {
  return arg.error_code() == status.error_code() &&
         arg.error_message() == status.error_message();
}

MATCHER_P(DerivedFromStatus, status, "") {
  if (arg.error_code() != status.error_code()) {
    return false;
  }
  if (arg.error_message().find(status.error_message()) == std::string::npos) {
    *result_listener << "\nOriginal error string: \"" << status.error_message()
                     << "\" is missing from the actual status.";
    return false;
  }
  return true;
}

constexpr uint64 kNodeId = 13579;
constexpr int kUnit = 2;
constexpr char kErrorMsg[] = "Test error message";
constexpr uint32 kPortId = 2468;

const std::map<uint64, int>& NodeIdToUnitMap() {
  static auto* map = new std::map<uint64, int>({{kNodeId, kUnit}});
  return *map;
}

class BcmSwitchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    phal_mock_ = absl::make_unique<PhalMock>();
    bcm_chassis_manager_mock_ = absl::make_unique<BcmChassisManagerMock>();
    bcm_node_mock_ = absl::make_unique<BcmNodeMock>();
    unit_to_bcm_node_mock_[kUnit] = bcm_node_mock_.get();
    bcm_switch_ = BcmSwitch::CreateInstance(phal_mock_.get(),
                                            bcm_chassis_manager_mock_.get(),
                                            unit_to_bcm_node_mock_);
    shutdown = false;  // global variable initialization

    ON_CALL(*bcm_chassis_manager_mock_, GetNodeIdToUnitMap())
        .WillByDefault(Return(NodeIdToUnitMap()));
  }

  void TearDown() override { unit_to_bcm_node_mock_.clear(); }

  void PushChassisConfigSuccess() {
    ChassisConfig config;
    config.add_nodes()->set_id(kNodeId);
    {
      InSequence sequence;  // The order of the calls are important. Enforce it.
      EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bcm_chassis_manager_mock_,
                  VerifyChassisConfig(EqualsProto(config)))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bcm_node_mock_,
                  VerifyChassisConfig(EqualsProto(config), kNodeId))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*phal_mock_, PushChassisConfig(EqualsProto(config)))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bcm_chassis_manager_mock_,
                  PushChassisConfig(EqualsProto(config)))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bcm_node_mock_,
                  PushChassisConfig(EqualsProto(config), kNodeId))
          .WillOnce(Return(::util::OkStatus()));
    }
    EXPECT_OK(bcm_switch_->PushChassisConfig(config));
  }

  ::util::Status DefaultError() {
    return ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, kErrorMsg);
  }

  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<BcmChassisManagerMock> bcm_chassis_manager_mock_;
  std::unique_ptr<BcmNodeMock> bcm_node_mock_;
  std::map<int, BcmNode*> unit_to_bcm_node_mock_;
  std::unique_ptr<BcmSwitch> bcm_switch_;
};

TEST_F(BcmSwitchTest, PushChassisConfigSuccess) { PushChassisConfigSuccess(); }

TEST_F(BcmSwitchTest, PushChassisConfigFailureWhenPhalVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(bcm_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, PushChassisConfigFailureWhenChassisManagerVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(bcm_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, PushChassisConfigFailureWhenNodeVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, PushChassisConfigFailureWhenPhalPushFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, PushChassisConfigFailureWhenChassisManagerPushFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, PushChassisConfigFailureWhenNodePushFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mock_, PushChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, VerifyChassisConfigSuccess) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  {
    InSequence sequence;  // The order of the calls are important. Enforce it.
    EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_chassis_manager_mock_,
                VerifyChassisConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_node_mock_,
                VerifyChassisConfig(EqualsProto(config), kNodeId))
        .WillOnce(Return(::util::OkStatus()));
  }
  EXPECT_OK(bcm_switch_->VerifyChassisConfig(config));
}

TEST_F(BcmSwitchTest, VerifyChassisConfigFailureWhenPhalVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(bcm_switch_->VerifyChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, VerifyChassisConfigFailureWhenChassisManagerVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(bcm_switch_->VerifyChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, VerifyChassisConfigFailureWhenNodeVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_switch_->VerifyChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest,
       VerifyChassisConfigFailureWhenMoreThanOneManagerVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bcm_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                                      "some other text")));

  // we keep the error code from the first error
  EXPECT_THAT(bcm_switch_->VerifyChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, ShutdownSuccess) {
  EXPECT_CALL(*bcm_node_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_switch_->Shutdown());
}

TEST_F(BcmSwitchTest, ShutdownFailureWhenSomeManagerShutdownFails) {
  EXPECT_CALL(*bcm_node_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_manager_mock_, Shutdown())
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*phal_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(bcm_switch_->Shutdown(), DerivedFromStatus(DefaultError()));
}

// PushForwardingPipelineConfig() should verify and propagate the config.
TEST_F(BcmSwitchTest, PushForwardingPipelineConfigSuccess) {
  PushChassisConfigSuccess();

  ::p4::v1::ForwardingPipelineConfig config;
  {
    InSequence sequence;
    // Verify should always be called before push.
    EXPECT_CALL(*bcm_node_mock_,
                VerifyForwardingPipelineConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_node_mock_,
                PushForwardingPipelineConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
  }
  EXPECT_OK(bcm_switch_->PushForwardingPipelineConfig(kNodeId, config));
}

// When BcmSwitchTest fails to verify a forwarding config during
// PushForwardingPipelineConfig(), it should not propagate the config and fail.
TEST_F(BcmSwitchTest, PushForwardingPipelineConfigFailureWhenVerifyFails) {
  PushChassisConfigSuccess();

  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_CALL(*bcm_node_mock_,
              VerifyForwardingPipelineConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bcm_node_mock_, PushForwardingPipelineConfig(_)).Times(0);
  EXPECT_THAT(bcm_switch_->PushForwardingPipelineConfig(kNodeId, config),
              DerivedFromStatus(DefaultError()));
}

// When BcmSwitchTest fails to push a forwarding config during
// PushForwardingPipelineConfig(), it should fail immediately.
TEST_F(BcmSwitchTest, PushForwardingPipelineConfigFailureWhenPushFails) {
  PushChassisConfigSuccess();

  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_CALL(*bcm_node_mock_,
              VerifyForwardingPipelineConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mock_,
              PushForwardingPipelineConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_THAT(bcm_switch_->PushForwardingPipelineConfig(kNodeId, config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmSwitchTest, VerifyForwardingPipelineConfigSuccess) {
  PushChassisConfigSuccess();

  ::p4::v1::ForwardingPipelineConfig config;
  {
    InSequence sequence;
    // Verify should always be called before push.
    EXPECT_CALL(*bcm_node_mock_,
                VerifyForwardingPipelineConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
  }
  EXPECT_OK(bcm_switch_->VerifyForwardingPipelineConfig(kNodeId, config));
}

TEST_F(BcmSwitchTest, WriteForwardingEntriesSuccess) {
  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  req.updates_size();
  std::vector<::util::Status> results = {};
  EXPECT_OK(bcm_switch_->WriteForwardingEntries(req, &results));
}

TEST_F(BcmSwitchTest, WriteForwardingEntriesDevIdFail) {
  ::p4::v1::WriteRequest req;
  req.add_updates();
  std::vector<::util::Status> results = {};
  ::util::Status status = bcm_switch_->WriteForwardingEntries(req, &results);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("No device_id in WriteRequest."));
}

TEST_F(BcmSwitchTest, WriteForwardingEntriesNullFail) {
  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  req.add_updates();
  ::util::Status status = bcm_switch_->WriteForwardingEntries(req, NULL);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr(
          "Need to provide non-null results pointer for non-empty updates."));
}

// Test registration of a writer for sending gNMI events.
TEST_F(BcmSwitchTest, RegisterEventNotifyWriterTest) {
  auto writer = std::shared_ptr<WriterInterface<GnmiEventPtr>>(
      new WriterMock<GnmiEventPtr>());

  EXPECT_CALL(*bcm_chassis_manager_mock_, RegisterEventNotifyWriter(writer))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(DefaultError()));

  // Successful BcmChassisManager registration.
  EXPECT_OK(bcm_switch_->RegisterEventNotifyWriter(writer));
  // Failed BcmChassisManager registration.
  EXPECT_THAT(bcm_switch_->RegisterEventNotifyWriter(writer),
              DerivedFromStatus(DefaultError()));
}

namespace {

void ExpectMockWriteDataResponse(WriterMock<DataResponse>* writer,
                                 DataResponse* resp) {
  // Mock implementation of Write() that saves the response to local variable.
  EXPECT_CALL(*writer, Write(_))
      .WillOnce(DoAll(WithArg<0>(Invoke([resp](DataResponse r) {
                        // Copy the response.
                        *resp = r;
                      })),
                      Return(true)));
}

}  // namespace

TEST_F(BcmSwitchTest, GetPortOperStatus) {
  PushChassisConfigSuccess();

  WriterMock<DataResponse> writer;
  DataResponse resp;

  // Expect successful retrieval followed by failure.
  ::util::Status error = ::util::UnknownErrorBuilder(GTL_LOC) << "error";
  EXPECT_CALL(*bcm_chassis_manager_mock_, GetPortState(kNodeId, kPortId))
      .WillOnce(Return(PORT_STATE_UP))
      .WillOnce(Return(error));
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_oper_status();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_oper_status());
  EXPECT_EQ(PORT_STATE_UP, resp.oper_status().state());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());

  details.clear();
  resp.Clear();
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_FALSE(resp.has_oper_status());
  ASSERT_EQ(details.size(), 1);
  EXPECT_EQ(error.ToString(), details.at(0).ToString());
}

TEST_F(BcmSwitchTest, GetPortAdminStatus) {
  PushChassisConfigSuccess();

  WriterMock<DataResponse> writer;
  DataResponse resp;

  // Expect successful retrieval followed by failure.
  ::util::Status error = ::util::UnknownErrorBuilder(GTL_LOC) << "error";
  EXPECT_CALL(*bcm_chassis_manager_mock_, GetPortAdminState(kNodeId, kPortId))
      .WillOnce(Return(ADMIN_STATE_ENABLED))
      .WillOnce(Return(error));
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_admin_status();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_admin_status());
  EXPECT_EQ(ADMIN_STATE_ENABLED, resp.admin_status().state());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());

  details.clear();
  resp.Clear();
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_FALSE(resp.has_admin_status());
  ASSERT_EQ(details.size(), 1);
  EXPECT_EQ(error.ToString(), details.at(0).ToString());
}

TEST_F(BcmSwitchTest, GetMacAddressPass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* request = req.add_requests()->mutable_mac_address();
  request->set_node_id(1);
  request->set_port_id(2);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_mac_address());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, GetPortSpeed) {
  PushChassisConfigSuccess();

  WriterMock<DataResponse> writer;
  DataResponse resp;

  // Expect successful retrieval followed by failure.
  const uint64 kSpeedBps = 100000000000;
  BcmPort port;
  port.set_speed_bps(kSpeedBps);
  ::util::Status error = ::util::UnknownErrorBuilder(GTL_LOC) << "error";
  EXPECT_CALL(*bcm_chassis_manager_mock_, GetBcmPort(kNodeId, kPortId))
      .WillOnce(Return(port))
      .WillOnce(Return(error));
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_port_speed();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_port_speed());
  EXPECT_EQ(kSpeedBps, resp.port_speed().speed_bps());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());

  details.clear();
  resp.Clear();
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_FALSE(resp.has_port_speed());
  ASSERT_EQ(details.size(), 1);
  EXPECT_EQ(error.ToString(), details.at(0).ToString());
}

TEST_F(BcmSwitchTest, GetMemoryErrorAlarmStatePass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  *req.add_requests()->mutable_memory_error_alarm() =
      DataRequest::Request::Chassis();
  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_memory_error_alarm());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, GetFlowProgrammingExceptionAlarmStatePass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  *req.add_requests()->mutable_flow_programming_exception_alarm() =
      DataRequest::Request::Chassis();
  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_flow_programming_exception_alarm());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, GetHealthIndicatorPass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* request = req.add_requests()->mutable_health_indicator();
  request->set_node_id(1);
  request->set_port_id(2);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_health_indicator());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, GetForwardingViablePass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* request = req.add_requests()->mutable_forwarding_viability();
  request->set_node_id(1);
  request->set_port_id(2);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_forwarding_viability());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, GetQosQueueCountersPass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* request = req.add_requests()->mutable_port_qos_counters();
  request->set_node_id(1);
  request->set_port_id(2);
  request->set_queue_id(4);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_port_qos_counters());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, GetNodePacketIoDebugInfoPass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* request = req.add_requests()->mutable_node_packetio_debug_info();
  request->set_node_id(1);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_node_packetio_debug_info());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, GetPortLoopbackStatus) {
  PushChassisConfigSuccess();

  WriterMock<DataResponse> writer;
  DataResponse resp;

  // Expect successful retrieval followed by failure.
  ::util::Status error = ::util::UnknownErrorBuilder(GTL_LOC) << "error";
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              GetPortLoopbackState(kNodeId, kPortId))
      .WillOnce(Return(LOOPBACK_STATE_NONE))
      .WillOnce(Return(error));
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_loopback_status();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_loopback_status());
  EXPECT_EQ(LOOPBACK_STATE_NONE, resp.loopback_status().state());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());

  details.clear();
  resp.Clear();
  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_FALSE(resp.has_loopback_status());
  ASSERT_EQ(details.size(), 1);
  EXPECT_EQ(error.ToString(), details.at(0).ToString());
}

TEST_F(BcmSwitchTest, GetSdnPortId) {
  PushChassisConfigSuccess();

  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_sdn_port_id();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(bcm_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_sdn_port_id());
  EXPECT_EQ(kPortId, resp.sdn_port_id().port_id());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, SetPortAdminStatusPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_admin_status()->set_state(AdminState::ADMIN_STATE_ENABLED);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, SetPortLoopbackStatusPass) {
  EXPECT_CALL(*bcm_chassis_manager_mock_,
              SetPortLoopbackState(1, 2, LOOPBACK_STATE_MAC))
      .WillOnce(Return(::util::OkStatus()));

  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_loopback_status()->set_state(
      LoopbackState::LOOPBACK_STATE_MAC);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, SetPortMacAddressPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_mac_address()->set_mac_address(0x112233445566);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, SetPortSpeedPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_port_speed()->set_speed_bps(40000000000);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, SetPortLacpSystemIdMacPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_lacp_router_mac()->set_mac_address(0x112233445566);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, SetPortLacpSystemPriorityPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_lacp_system_priority()->set_priority(10);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, SetPortHealthIndicatorPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_health_indicator()->set_state(HealthState::HEALTH_STATE_BAD);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(BcmSwitchTest, SetPortNoContentsPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);

  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->SetValue(/* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0).ToString(), HasSubstr("Not supported yet"));
}

TEST_F(BcmSwitchTest, SetNoContentsPass) {
  SetRequest req;
  req.add_requests();
  std::vector<::util::Status> details;
  EXPECT_OK(bcm_switch_->SetValue(/* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0).ToString(), HasSubstr("Not supported yet"));
}

// TODO(unknown): Complete unit test coverage.

}  // namespace
}  // namespace bcm
}  // namespace hal
}  // namespace stratum
