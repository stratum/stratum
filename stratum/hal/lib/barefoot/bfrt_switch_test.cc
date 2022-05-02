// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_switch.h"

#include <map>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/canonical_errors.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/barefoot/bf_chassis_manager_mock.h"
#include "stratum/hal/lib/barefoot/bf_sde_mock.h"
#include "stratum/hal/lib/barefoot/bfrt_node_mock.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

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
constexpr int kDevice = 2;
constexpr char kErrorMsg[] = "Test error message";
constexpr uint32 kPortId = 2468;

const std::map<uint64, int>& NodeIdToDeviceMap() {
  static auto* map = new std::map<uint64, int>({{kNodeId, kDevice}});
  return *map;
}

class BfrtSwitchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    phal_mock_ = absl::make_unique<PhalMock>();
    bf_sde_mock_ = absl::make_unique<BfSdeMock>();
    bf_chassis_manager_mock_ = absl::make_unique<BfChassisManagerMock>();
    bfrt_node_mock_ = absl::make_unique<BfrtNodeMock>();
    device_to_bfrt_node_mock_[kDevice] = bfrt_node_mock_.get();
    bfrt_switch_ = BfrtSwitch::CreateInstance(
        phal_mock_.get(), bf_chassis_manager_mock_.get(), bf_sde_mock_.get(),
        device_to_bfrt_node_mock_);

    ON_CALL(*bf_chassis_manager_mock_, GetNodeIdToDeviceMap())
        .WillByDefault(Return(NodeIdToDeviceMap()));
  }

  void TearDown() override { device_to_bfrt_node_mock_.clear(); }

  void PushChassisConfigSuccess() {
    ChassisConfig config;
    config.add_nodes()->set_id(kNodeId);
    {
      InSequence sequence;  // The order of the calls are important. Enforce it.
      EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bf_chassis_manager_mock_,
                  VerifyChassisConfig(EqualsProto(config)))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bfrt_node_mock_,
                  VerifyChassisConfig(EqualsProto(config), kNodeId))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*phal_mock_, PushChassisConfig(EqualsProto(config)))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bf_chassis_manager_mock_,
                  PushChassisConfig(EqualsProto(config)))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bfrt_node_mock_,
                  PushChassisConfig(EqualsProto(config), kNodeId))
          .WillOnce(Return(::util::OkStatus()));
    }
    EXPECT_OK(bfrt_switch_->PushChassisConfig(config));
  }

  ::util::Status DefaultError() {
    return ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, kErrorMsg);
  }

 protected:
  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<BfSdeMock> bf_sde_mock_;
  std::unique_ptr<BfChassisManagerMock> bf_chassis_manager_mock_;
  std::unique_ptr<BfrtNodeMock> bfrt_node_mock_;
  absl::flat_hash_map<int, BfrtNode*> device_to_bfrt_node_mock_;
  std::unique_ptr<BfrtSwitch> bfrt_switch_;
};

TEST_F(BfrtSwitchTest, PushChassisConfigSuccess) { PushChassisConfigSuccess(); }

TEST_F(BfrtSwitchTest, PushChassisConfigFailureWhenPhalVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bf_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(bfrt_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest, PushChassisConfigFailureWhenNodeVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bfrt_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest, PushChassisConfigFailureWhenPhalPushFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bfrt_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest, PushChassisConfigFailureWhenChassisManagerPushFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bfrt_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest, PushChassisConfigFailureWhenNodePushFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bfrt_node_mock_, PushChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bfrt_switch_->PushChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest, VerifyChassisConfigSuccess) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  {
    InSequence sequence;  // The order of the calls are important. Enforce it.
    EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bf_chassis_manager_mock_,
                VerifyChassisConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bfrt_node_mock_,
                VerifyChassisConfig(EqualsProto(config), kNodeId))
        .WillOnce(Return(::util::OkStatus()));
  }
  EXPECT_OK(bfrt_switch_->VerifyChassisConfig(config));
}

TEST_F(BfrtSwitchTest, VerifyChassisConfigFailureWhenPhalVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bf_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(bfrt_switch_->VerifyChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest,
       VerifyChassisConfigFailureWhenChassisManagerVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(bfrt_switch_->VerifyChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest, VerifyChassisConfigFailureWhenNodeVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bfrt_switch_->VerifyChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest,
       VerifyChassisConfigFailureWhenMoreThanOneManagerVerifyFails) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_CALL(*phal_mock_, VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_,
              VerifyChassisConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyChassisConfig(EqualsProto(config), kNodeId))
      .WillOnce(Return(::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                                      "some other text")));

  // we keep the error code from the first error
  EXPECT_THAT(bfrt_switch_->VerifyChassisConfig(config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest, ShutdownSuccess) {
  EXPECT_CALL(*bfrt_node_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bfrt_switch_->Shutdown());
}

TEST_F(BfrtSwitchTest, ShutdownFailureWhenSomeManagerShutdownFails) {
  EXPECT_CALL(*bfrt_node_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_chassis_manager_mock_, Shutdown())
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*phal_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(bfrt_switch_->Shutdown(), DerivedFromStatus(DefaultError()));
}

// PushForwardingPipelineConfig() should verify and propagate the config.
TEST_F(BfrtSwitchTest, PushForwardingPipelineConfigSuccess) {
  PushChassisConfigSuccess();

  ::p4::v1::ForwardingPipelineConfig config;
  {
    InSequence sequence;
    EXPECT_CALL(*bfrt_node_mock_,
                VerifyForwardingPipelineConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bfrt_node_mock_,
                PushForwardingPipelineConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
  }
  EXPECT_OK(bfrt_switch_->PushForwardingPipelineConfig(kNodeId, config));
}

// When BfrtSwitch fails to verify a forwarding config during
// PushForwardingPipelineConfig(), it should not propagate the config and fail.
TEST_F(BfrtSwitchTest, PushForwardingPipelineConfigFailureWhenVerifyFails) {
  PushChassisConfigSuccess();

  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyForwardingPipelineConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bfrt_node_mock_, PushForwardingPipelineConfig(_)).Times(0);
  EXPECT_THAT(bfrt_switch_->PushForwardingPipelineConfig(kNodeId, config),
              DerivedFromStatus(DefaultError()));
}

// When BfrtSwitch fails to push a forwarding config during
// PushForwardingPipelineConfig(), it should fail immediately.
TEST_F(BfrtSwitchTest, PushForwardingPipelineConfigFailureWhenPushFails) {
  PushChassisConfigSuccess();

  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_CALL(*bfrt_node_mock_,
              VerifyForwardingPipelineConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bfrt_node_mock_,
              PushForwardingPipelineConfig(EqualsProto(config)))
      .WillOnce(Return(DefaultError()));
  EXPECT_THAT(bfrt_switch_->PushForwardingPipelineConfig(kNodeId, config),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BfrtSwitchTest, VerifyForwardingPipelineConfigSuccess) {
  PushChassisConfigSuccess();

  ::p4::v1::ForwardingPipelineConfig config;
  {
    InSequence sequence;
    // Verify should always be called before push.
    EXPECT_CALL(*bfrt_node_mock_,
                VerifyForwardingPipelineConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
  }
  EXPECT_OK(bfrt_switch_->VerifyForwardingPipelineConfig(kNodeId, config));
}

// Test registration of a writer for sending gNMI events.
TEST_F(BfrtSwitchTest, RegisterEventNotifyWriterTest) {
  auto writer = std::shared_ptr<WriterInterface<GnmiEventPtr>>(
      new WriterMock<GnmiEventPtr>());

  EXPECT_CALL(*bf_chassis_manager_mock_, RegisterEventNotifyWriter(writer))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(DefaultError()));

  // Successful BfChassisManager registration.
  EXPECT_OK(bfrt_switch_->RegisterEventNotifyWriter(writer));
  // Failed BfChassisManager registration.
  EXPECT_THAT(bfrt_switch_->RegisterEventNotifyWriter(writer),
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

TEST_F(BfrtSwitchTest, RetrieveValueNodeInfo) {
  constexpr char kTofinoChipTypeString[] = "T32-X";

  PushChassisConfigSuccess();

  WriterMock<DataResponse> writer;
  DataResponse resp;

  // Expect successful retrieval followed by failure.
  ::util::Status error = ::util::UnknownErrorBuilder(GTL_LOC) << "error";
  EXPECT_CALL(*bf_chassis_manager_mock_, GetDeviceFromNodeId(kNodeId))
      .WillOnce(Return(kDevice))
      .WillOnce(Return(error));
  ExpectMockWriteDataResponse(&writer, &resp);

  EXPECT_CALL(*bf_sde_mock_, GetBfChipType(kDevice))
      .WillOnce(Return(kTofinoChipTypeString));

  DataRequest req;
  auto* req_info = req.add_requests()->mutable_node_info();
  req_info->set_node_id(kNodeId);
  std::vector<::util::Status> details;

  EXPECT_OK(bfrt_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_TRUE(resp.has_node_info());
  EXPECT_EQ("Barefoot", resp.node_info().vendor_name());
  EXPECT_EQ(kTofinoChipTypeString, resp.node_info().chip_name());
  ASSERT_EQ(details.size(), 1);
  EXPECT_THAT(details.at(0), ::util::OkStatus());

  details.clear();
  resp.Clear();
  EXPECT_OK(bfrt_switch_->RetrieveValue(kNodeId, req, &writer, &details));
  EXPECT_FALSE(resp.has_node_info());
  ASSERT_EQ(details.size(), 1);
  EXPECT_EQ(error.ToString(), details.at(0).ToString());
}

// TODO(max): add more tests, use BcmSwitch as a reference.

}  // namespace

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
