// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// adapted from ipdk_switch_test, which was
// adapted from bcm_switch_test

#include "stratum/hal/lib/tdi/tofino/tofino_switch.h"

#include <utility>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/canonical_errors.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/hal/lib/p4/p4_table_mapper_mock.h"
#include "stratum/hal/lib/tdi/tdi_node_mock.h"
#include "stratum/hal/lib/tdi/tdi_sde_mock.h"
#include "stratum/hal/lib/tdi/tofino/tofino_chassis_manager_mock.h"
#include "stratum/lib/channel/channel_mock.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace tdi {

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
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

class TofinoSwitchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use NiceMock to suppress "uninteresting mock function call" warnings
    phal_mock_ = absl::make_unique<NiceMock<PhalMock>>();
    sde_mock_ = absl::make_unique<NiceMock<TdiSdeMock>>();
    chassis_manager_mock_ = absl::make_unique<NiceMock<TofinoChassisManagerMock>>();
    node_mock_ = absl::make_unique<NiceMock<TdiNodeMock>>();
    unit_to_ipdk_node_mock_[kUnit] = node_mock_.get();
    switch_ = TofinoSwitch::CreateInstance(
        phal_mock_.get(),
        chassis_manager_mock_.get(),
        sde_mock_.get(),
        unit_to_ipdk_node_mock_);
#if 0
    // no 'shutdown'
    shutdown = false;  // global variable initialization
#endif

    ON_CALL(*chassis_manager_mock_, GetNodeIdToUnitMap())
        .WillByDefault(Return(NodeIdToUnitMap()));
  }

  void TearDown() override { unit_to_ipdk_node_mock_.clear(); }

  // This operation should always succeed.
  // We use it to set up a number of test cases.
  void PushChassisConfigSuccessfully() {
    ChassisConfig config;
    config.add_nodes()->set_id(kNodeId);
    EXPECT_CALL(*node_mock_,
                PushChassisConfig(EqualsProto(config), kNodeId))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_OK(switch_->PushChassisConfig(config));
  }

  ::util::Status DefaultError() {
    return ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, kErrorMsg);
  }

  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<TdiSdeMock> sde_mock_;
  std::unique_ptr<TofinoChassisManagerMock> chassis_manager_mock_;
  std::unique_ptr<TdiNodeMock> node_mock_;
  std::map<int, TdiNode*> unit_to_ipdk_node_mock_;
  std::unique_ptr<TofinoSwitch> switch_;
};

TEST_F(TofinoSwitchTest, PushChassisConfigSucceeds) {
    PushChassisConfigSuccessfully();
}

TEST_F(TofinoSwitchTest, PushChassisConfigFailsWhenNodePushFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*node_mock_, PushChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(TofinoSwitchTest, VerifyChassisConfigSucceeds) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_OK(switch_->VerifyChassisConfig(config));
}

TEST_F(TofinoSwitchTest, ShutdownSucceeds) {
  EXPECT_CALL(*node_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*chassis_manager_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(switch_->Shutdown());
}

TEST_F(TofinoSwitchTest, ShutdownFailsWhenSomeManagerShutdownFails) {
  EXPECT_CALL(*node_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*chassis_manager_mock_, Shutdown())
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*phal_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(switch_->Shutdown(), DerivedFromStatus(DefaultError()));
}

#if 0
//
// Chassis config pushed successfully.
// P4-based forwarding pipeline config pushed successfully to node with ID 13579.
// Return Error: tdi_sde_interface_->GetPcieCpuPort(device_id) at stratum/hal/lib/tdi/tdi_switch.cc:91
//
// stratum/hal/lib/tdi/tdi_switch_test.cc:166: Failure
// Value of: switch_->PushForwardingPipelineConfig(kNodeId, config)
// Expected: is OK
//  Actual: generic::unknown:  (of type util::Status)
//
// PushForwardingPipelineConfig() should propagate the config.
TEST_F(TofinoSwitchTest, PushForwardingPipelineConfigSucceeds) {
  PushChassisConfigSuccessfully();

  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_CALL(*node_mock_,
              PushForwardingPipelineConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(switch_->PushForwardingPipelineConfig(kNodeId, config));
}
#endif

// When TofinoSwitchTest fails to push a forwarding config during
// PushForwardingPipelineConfig(), it should fail immediately.
TEST_F(TofinoSwitchTest, PushForwardingPipelineConfigFailsWhenPushFails) {
  PushChassisConfigSuccessfully();

  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_CALL(*node_mock_,
              PushForwardingPipelineConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_THAT(switch_->PushForwardingPipelineConfig(kNodeId, config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(TofinoSwitchTest, VerifyForwardingPipelineConfigSucceeds) {
  PushChassisConfigSuccessfully();

  ::p4::v1::ForwardingPipelineConfig config;
  // Verify should always be called before push.
  EXPECT_CALL(*node_mock_,
              VerifyForwardingPipelineConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(switch_->VerifyForwardingPipelineConfig(kNodeId, config));
}

// Test registration of a writer for sending gNMI events.
TEST_F(TofinoSwitchTest, RegisterEventNotifyWriterTest) {
  auto writer = std::shared_ptr<WriterInterface<GnmiEventPtr>>(
      new WriterMock<GnmiEventPtr>());

  EXPECT_CALL(*chassis_manager_mock_, RegisterEventNotifyWriter(writer))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(DefaultError()));

  // Successful TofinoChassisManager registration.
  EXPECT_OK(switch_->RegisterEventNotifyWriter(writer));
  // Failed TofinoChassisManager registration.
  EXPECT_THAT(switch_->RegisterEventNotifyWriter(writer),
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

#if 0
// No GetPortState()
TEST_F(TofinoSwitchTest, GetPortOperStatus) {
  PushChassisConfigSuccessfully();

  WriterMock<DataResponse> writer;
  DataResponse resp;

  // Expect successful retrieval followed by failure.
  ::util::Status error = ::util::UnknownErrorBuilder(GTL_LOC) << "error";
  EXPECT_CALL(*chassis_manager_mock_, GetPortState(kNodeId, kPortId))
      .WillOnce(Return(PORT_STATE_UP))
      .WillOnce(Return(error));
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_oper_status();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_oper_status());
  EXPECT_EQ(PORT_STATE_UP, resp.oper_status().state());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());

  details.clear();
  resp.Clear();
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_FALSE(resp.has_oper_status());
  ASSERT_EQ(details.size(), 1);
  EXPECT_EQ(error.ToString(), details.at(0).ToString());
}
#endif

#if 0
// No GetPortAdminState()
TEST_F(TofinoSwitchTest, GetPortAdminStatus) {
  PushChassisConfigSuccessfully();

  WriterMock<DataResponse> writer;
  DataResponse resp;

  // Expect successful retrieval followed by failure.
  ::util::Status error = ::util::UnknownErrorBuilder(GTL_LOC) << "error";
  EXPECT_CALL(*chassis_manager_mock_, GetPortAdminState(kNodeId, kPortId))
      .WillOnce(Return(ADMIN_STATE_ENABLED))
      .WillOnce(Return(error));
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_admin_status();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_admin_status());
  EXPECT_EQ(ADMIN_STATE_ENABLED, resp.admin_status().state());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());

  details.clear();
  resp.Clear();
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_FALSE(resp.has_admin_status());
  ASSERT_EQ(details.size(), 1);
  EXPECT_EQ(error.ToString(), details.at(0).ToString());
}
#endif

#if 0
// mac_address() not returned
TEST_F(TofinoSwitchTest, GetMacAddressPass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* request = req.add_requests()->mutable_mac_address();
  request->set_node_id(1);
  request->set_port_id(2);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_mac_address());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}
#endif

#if 0
// No BcmPort
TEST_F(TofinoSwitchTest, GetPortSpeed) {
  PushChassisConfigSuccessfully();

  WriterMock<DataResponse> writer;
  DataResponse resp;

  // Expect successful retrieval followed by failure.
  const uint64 kSpeedBps = 100000000000;
  BcmPort port;
  port.set_speed_bps(kSpeedBps);
  ::util::Status error = ::util::UnknownErrorBuilder(GTL_LOC) << "error";
  EXPECT_CALL(*chassis_manager_mock_, GetBcmPort(kNodeId, kPortId))
      .WillOnce(Return(port))
      .WillOnce(Return(error));
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_port_speed();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_port_speed());
  EXPECT_EQ(kSpeedBps, resp.port_speed().speed_bps());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());

  details.clear();
  resp.Clear();
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_FALSE(resp.has_port_speed());
  ASSERT_EQ(details.size(), 1);
  EXPECT_EQ(error.ToString(), details.at(0).ToString());
}
#endif

TEST_F(TofinoSwitchTest, GetMemoryErrorAlarmStatePass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
#if 0
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);
#endif

  DataRequest req;
  *req.add_requests()->mutable_memory_error_alarm() =
      DataRequest::Request::Chassis();
  std::vector<::util::Status> details;
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
#if 0
  EXPECT_TRUE(resp.has_memory_error_alarm());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
#else
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  ASSERT_GE(details.size(), 1);
  ASSERT_EQ(details.at(0).error_code(), ERR_UNIMPLEMENTED);
#endif
}

#if 0

TEST_F(TofinoSwitchTest, GetFlowProgrammingExceptionAlarmStatePass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  *req.add_requests()->mutable_flow_programming_exception_alarm() =
      DataRequest::Request::Chassis();
  std::vector<::util::Status> details;
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_flow_programming_exception_alarm());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

// Doesn't work
TEST_F(TofinoSwitchTest, GetHealthIndicatorPass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* request = req.add_requests()->mutable_health_indicator();
  request->set_node_id(1);
  request->set_port_id(2);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_health_indicator());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

// Doesn't work
TEST_F(TofinoSwitchTest, GetForwardingViablePass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* request = req.add_requests()->mutable_forwarding_viability();
  request->set_node_id(1);
  request->set_port_id(2);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_forwarding_viability());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}
=
TEST_F(TofinoSwitchTest, GetQosQueueCountersPass) {
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
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_port_qos_counters());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(TofinoSwitchTest, GetNodePacketIoDebugInfoPass) {
  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* request = req.add_requests()->mutable_node_packetio_debug_info();
  request->set_node_id(1);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_node_packetio_debug_info());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

#if 0
// No GetPortLoopbackState()
TEST_F(TofinoSwitchTest, GetPortLoopbackStatus) {
  PushChassisConfigSuccessfully();

  WriterMock<DataResponse> writer;
  DataResponse resp;

  // Expect successful retrieval followed by failure.
  ::util::Status error = ::util::UnknownErrorBuilder(GTL_LOC) << "error";
  EXPECT_CALL(*chassis_manager_mock_,
              GetPortLoopbackState(kNodeId, kPortId))
      .WillOnce(Return(LOOPBACK_STATE_NONE))
      .WillOnce(Return(error));
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_loopback_status();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_loopback_status());
  EXPECT_EQ(LOOPBACK_STATE_NONE, resp.loopback_status().state());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());

  details.clear();
  resp.Clear();
  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_FALSE(resp.has_loopback_status());
  ASSERT_EQ(details.size(), 1);
  EXPECT_EQ(error.ToString(), details.at(0).ToString());
}
#endif

TEST_F(TofinoSwitchTest, GetSdnPortId) {
  PushChassisConfigSuccessfully();

  WriterMock<DataResponse> writer;
  DataResponse resp;
  // Expect Write() call and store data in resp.
  ExpectMockWriteDataResponse(&writer, &resp);

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_sdn_port_id();
  req_info->set_node_id(kNodeId);
  req_info->set_port_id(kPortId);
  std::vector<::util::Status> details;

  EXPECT_OK(switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_sdn_port_id());
  EXPECT_EQ(kPortId, resp.sdn_port_id().port_id());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(TofinoSwitchTest, SetPortAdminStatusPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_admin_status()->set_state(AdminState::ADMIN_STATE_ENABLED);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

#if 0
// No SetPortLoopbackState()
TEST_F(TofinoSwitchTest, SetPortLoopbackStatusPass) {
  EXPECT_CALL(*chassis_manager_mock_,
              SetPortLoopbackState(1, 2, LOOPBACK_STATE_MAC))
      .WillOnce(Return(::util::OkStatus()));

  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_loopback_status()->set_state(
      LoopbackState::LOOPBACK_STATE_MAC);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}
#endif

TEST_F(TofinoSwitchTest, SetPortMacAddressPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_mac_address()->set_mac_address(0x112233445566);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(TofinoSwitchTest, SetPortSpeedPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_port_speed()->set_speed_bps(40000000000);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(TofinoSwitchTest, SetPortLacpSystemIdMacPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_lacp_router_mac()->set_mac_address(0x112233445566);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(TofinoSwitchTest, SetPortLacpSystemPriorityPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_lacp_system_priority()->set_priority(10);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(TofinoSwitchTest, SetPortHealthIndicatorPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);
  request->mutable_health_indicator()->set_state(HealthState::HEALTH_STATE_BAD);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->SetValue(
      /* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());
}

TEST_F(TofinoSwitchTest, SetPortNoContentsPass) {
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(1);
  request->set_port_id(2);

  std::vector<::util::Status> details;
  EXPECT_OK(switch_->SetValue(/* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0).ToString(), HasSubstr("Not supported yet"));
}

TEST_F(TofinoSwitchTest, SetNoContentsPass) {
  SetRequest req;
  req.add_requests();
  std::vector<::util::Status> details;
  EXPECT_OK(switch_->SetValue(/* node_id */ 0, req, &details));
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0).ToString(), HasSubstr("Not supported yet"));
}
#endif

// TODO(unknown): Complete unit test coverage.

}  // namespace
}  // namespace tdi
}  // namespace hal
}  // namespace stratum
