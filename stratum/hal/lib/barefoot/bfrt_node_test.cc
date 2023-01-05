// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_node.h"

#include <string>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/canonical_errors.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/barefoot/bf_sde_mock.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/bfrt_counter_manager_mock.h"
#include "stratum/hal/lib/barefoot/bfrt_p4runtime_translator_mock.h"
#include "stratum/hal/lib/barefoot/bfrt_packetio_manager_mock.h"
#include "stratum/hal/lib/barefoot/bfrt_pre_manager_mock.h"
#include "stratum/hal/lib/barefoot/bfrt_table_manager_mock.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

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

class BfrtNodeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bfrt_table_manager_mock_ = absl::make_unique<BfrtTableManagerMock>();
    bfrt_packetio_manager_mock_ = absl::make_unique<BfrtPacketioManagerMock>();
    bfrt_pre_manager_mock_ = absl::make_unique<BfrtPreManagerMock>();
    bfrt_counter_manager_mock_ = absl::make_unique<BfrtCounterManagerMock>();
    bf_sde_mock_ = absl::make_unique<BfSdeMock>();
    bfrt_p4runtime_translator_mock_ =
        absl::make_unique<BfrtP4RuntimeTranslatorMock>();

    bfrt_node_ = BfrtNode::CreateInstance(
        bfrt_table_manager_mock_.get(), bfrt_packetio_manager_mock_.get(),
        bfrt_pre_manager_mock_.get(), bfrt_counter_manager_mock_.get(),
        bfrt_p4runtime_translator_mock_.get(), bf_sde_mock_.get(), kDeviceId);
  }

  ::util::Status PushChassisConfig(const ChassisConfig& config,
                                   uint64 node_id) {
    return bfrt_node_->PushChassisConfig(config, node_id);
  }

  ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                     uint64 node_id) {
    return bfrt_node_->VerifyChassisConfig(config, node_id);
  }

  ::util::Status PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config) {
    return bfrt_node_->PushForwardingPipelineConfig(config);
  }

  ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config) {
    return bfrt_node_->VerifyForwardingPipelineConfig(config);
  }

  ::util::Status WriteForwardingEntries(const ::p4::v1::WriteRequest& req,
                                        std::vector<::util::Status>* results) {
    return bfrt_node_->WriteForwardingEntries(req, results);
  }

  ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* results) {
    return bfrt_node_->ReadForwardingEntries(req, writer, results);
  }

  ::util::Status RegisterStreamMessageResponseWriter(
      const std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>>&
          writer) {
    return bfrt_node_->RegisterStreamMessageResponseWriter(writer);
  }

  ::util::Status UnregisterStreamMessageResponseWriter() {
    return bfrt_node_->UnregisterStreamMessageResponseWriter();
  }

  ::util::Status HandleStreamMessageRequest(
      const ::p4::v1::StreamMessageRequest& req) {
    return bfrt_node_->HandleStreamMessageRequest(req);
  }

  void PushChassisConfigWithCheck() {
    ChassisConfig config;
    config.add_nodes()->set_id(kNodeId);
    {
      InSequence sequence;  // The order of the calls are important. Enforce it.
      // EXPECT_CALL(*p4_table_mapper_mock_,
      //             PushChassisConfig(EqualsProto(config), kNodeId))
      //     .WillOnce(Return(::util::OkStatus()));
      // EXPECT_CALL(*bfrt_table_manager_mock_,
      //             PushChassisConfig(EqualsProto(config), kNodeId))
      //     .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bfrt_packetio_manager_mock_,
                  PushChassisConfig(EqualsProto(config), kNodeId))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
                  PushChassisConfig(EqualsProto(config), kNodeId))
          .WillOnce(Return(::util::OkStatus()));
      // EXPECT_CALL(*bfrt_pre_manager_mock_,
      //             PushChassisConfig(EqualsProto(config), kNodeId))
      //     .WillOnce(Return(::util::OkStatus()));
      // EXPECT_CALL(*bfrt_counter_manager_mock_,
      //             PushChassisConfig(EqualsProto(config), kNodeId))
      //     .WillOnce(Return(::util::OkStatus()));
    }
    ASSERT_OK(PushChassisConfig(config, kNodeId));
    ASSERT_TRUE(IsInitialized());
  }

  void PushForwardingPipelineConfigWithCheck() {
    ::p4::v1::ForwardingPipelineConfig config =
        GetDefaultForwardingPipelineConfig();
    {
      InSequence sequence;
      // TODO(max): match on passed config
      EXPECT_CALL(*bfrt_table_manager_mock_, VerifyForwardingPipelineConfig(_))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bf_sde_mock_, AddDevice(kDeviceId, _))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
                  PushForwardingPipelineConfig(_))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bfrt_packetio_manager_mock_, PushForwardingPipelineConfig(_))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bfrt_table_manager_mock_, PushForwardingPipelineConfig(_))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bfrt_pre_manager_mock_, PushForwardingPipelineConfig(_))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bfrt_counter_manager_mock_, PushForwardingPipelineConfig(_))
          .WillOnce(Return(::util::OkStatus()));
    }
    EXPECT_OK(PushForwardingPipelineConfig(config));
    ASSERT_TRUE(IsPipelineInitialized());
  }

  bool IsInitialized() {
    absl::WriterMutexLock l(&bfrt_node_->lock_);
    return bfrt_node_->initialized_;
  }

  bool IsPipelineInitialized() {
    absl::WriterMutexLock l(&bfrt_node_->lock_);
    return bfrt_node_->pipeline_initialized_;
  }

  ::util::Status DefaultError() {
    return ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, kErrorMsg);
  }

  ::p4::v1::ForwardingPipelineConfig GetDefaultForwardingPipelineConfig() {
    ::p4::v1::ForwardingPipelineConfig config;
    BfPipelineConfig bf_config;
    CHECK_OK(ParseProtoFromString(kBfConfigPipelineString, &bf_config));
    std::string bf_config_bytes;
    CHECK(bf_config.SerializeToString(&bf_config_bytes));
    config.set_p4_device_config(bf_config_bytes);
    CHECK_OK(ParseProtoFromString(kValidP4InfoString, config.mutable_p4info()));

    return config;
  }

  static constexpr uint64 kNodeId = 13579;
  static constexpr int kDeviceId = 2;
  static constexpr char kErrorMsg[] = "Test error message";
  static constexpr uint32 kMemberId = 841;
  static constexpr uint32 kGroupId = 111;
  static constexpr int kEgressIntfId = 10001;
  static constexpr int kLogicalPortId = 35;
  static constexpr uint32 kPortId = 941;
  static constexpr uint32 kL2McastGroupId = 20;
  static constexpr char kBfConfigPipelineString[] = R"pb(
    p4_name: "prog1"
    bfruntime_info: "{json: true}"
    profiles {
      profile_name: "pipe1"
      context: "{json: true}"
      binary: "<raw bin>"
    }
  )pb";
  static constexpr char kValidP4InfoString[] = R"pb(
    pkg_info {
      arch: "tna"
    }
    tables {
      preamble {
        id: 33583783
        name: "Ingress.control.table1"
      }
      match_fields {
        id: 1
        name: "field1"
        bitwidth: 9
        match_type: EXACT
      }
      match_fields {
        id: 2
        name: "field2"
        bitwidth: 12
        match_type: TERNARY
      }
      match_fields {
        id: 3
        name: "field3"
        bitwidth: 15
        match_type: RANGE
      }
      action_refs {
        id: 16794911
      }
      const_default_action_id: 16836487
      direct_resource_ids: 318814845
      size: 1024
    }
    actions {
      preamble {
        id: 16794911
        name: "Ingress.control.action1"
      }
      params {
        id: 1
        name: "vlan_id"
        bitwidth: 12
      }
    }
    direct_counters {
      preamble {
        id: 318814845
        name: "Ingress.control.counter1"
      }
      spec {
        unit: BOTH
      }
      direct_table_id: 33583783
    }
    meters {
      preamble {
        id: 55555
        name: "Ingress.control.meter_bytes"
        alias: "meter_bytes"
      }
      spec {
        unit: BYTES
      }
      size: 500
    }
    meters {
      preamble {
        id: 55556
        name: "Ingress.control.meter_packets"
        alias: "meter_packets"
      }
      spec {
        unit: PACKETS
      }
      size: 500
    }
  )pb";

  std::unique_ptr<BfrtTableManagerMock> bfrt_table_manager_mock_;
  std::unique_ptr<BfrtPacketioManagerMock> bfrt_packetio_manager_mock_;
  std::unique_ptr<BfrtPreManagerMock> bfrt_pre_manager_mock_;
  std::unique_ptr<BfrtCounterManagerMock> bfrt_counter_manager_mock_;
  std::unique_ptr<BfSdeMock> bf_sde_mock_;
  std::unique_ptr<BfrtNode> bfrt_node_;
  std::unique_ptr<BfrtP4RuntimeTranslatorMock> bfrt_p4runtime_translator_mock_;
};

constexpr uint64 BfrtNodeTest::kNodeId;
constexpr int BfrtNodeTest::kDeviceId;
constexpr char BfrtNodeTest::kErrorMsg[];
constexpr uint32 BfrtNodeTest::kMemberId;
constexpr uint32 BfrtNodeTest::kGroupId;
constexpr int BfrtNodeTest::kEgressIntfId;
constexpr int BfrtNodeTest::kLogicalPortId;
constexpr uint32 BfrtNodeTest::kPortId;
constexpr char BfrtNodeTest::kBfConfigPipelineString[];
constexpr char BfrtNodeTest::kValidP4InfoString[];

TEST_F(BfrtNodeTest, PushChassisConfigSuccess) { PushChassisConfigWithCheck(); }

// TEST_F(BfrtNodeTest, PushChassisConfigFailureWhenTableMapperPushFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));

//   EXPECT_THAT(PushChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, PushChassisConfigFailureWhenTableManagerPushFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));

//   EXPECT_THAT(PushChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, PushChassisConfigFailureWhenL2ManagerPushFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));

//   EXPECT_THAT(PushChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, PushChassisConfigFailureWhenL3ManagerPushFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));

//   EXPECT_THAT(PushChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, PushChassisConfigFailureWhenAclManagerPushFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));

//   EXPECT_THAT(PushChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, PushChassisConfigFailureWhenTunnelManagerPushFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));

//   EXPECT_THAT(PushChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, PushChassisConfigFailureWhenPacketioManagerPushFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               PushChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));

//   EXPECT_THAT(PushChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

TEST_F(BfrtNodeTest, VerifyChassisConfigSuccess) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  {
    InSequence sequence;  // The order of the calls are important. Enforce it.
    EXPECT_CALL(*bfrt_packetio_manager_mock_,
                VerifyChassisConfig(EqualsProto(config), kNodeId))
        .WillOnce(Return(::util::OkStatus()));
  }
  EXPECT_OK(VerifyChassisConfig(config, kNodeId));
  EXPECT_FALSE(IsInitialized());  // Should be false even of verify passes
}

// TEST_F(BfrtNodeTest, VerifyChassisConfigFailureWhenTableMapperVerifyFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));

//   EXPECT_THAT(VerifyChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, VerifyChassisConfigFailureWhenTableManagerVerifyFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));

//   EXPECT_THAT(VerifyChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, VerifyChassisConfigFailureWhenL2ManagerVerifyFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));

//   EXPECT_THAT(VerifyChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, VerifyChassisConfigFailureWhenL3ManagerVerifyFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));

//   EXPECT_THAT(VerifyChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, VerifyChassisConfigFailureWhenAclManagerrVerifyFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));

//   EXPECT_THAT(VerifyChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, VerifyChassisConfigFailureWhenTunnelManagerrVerifyFails)
// {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));

//   EXPECT_THAT(VerifyChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest,
// VerifyChassisConfigFailureWhenPacketioManagerVerifyFails) {
//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));

//   EXPECT_THAT(VerifyChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_FALSE(IsInitialized());
// }

// TEST_F(BfrtNodeTest, VerifyChassisConfigFailureWhenMultiManagerVerifyFails) {
//   ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(DefaultError()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId))
//       .WillOnce(
//           Return(::util::Status(StratumErrorSpace(), ERR_INTERNAL,
//           kErrorMsg)));

//   EXPECT_THAT(VerifyChassisConfig(config, kNodeId),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_TRUE(IsInitialized());  // Initialized as we pushed config before
// }

// TEST_F(BfrtNodeTest, VerifyChassisConfigFailureForInvalidNodeId) {
//   ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), 0))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), 0))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), 0))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), 0))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), 0))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), 0))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), 0))
//       .WillOnce(Return(::util::OkStatus()));

//   ::util::Status status = VerifyChassisConfig(config, 0);
//   ASSERT_FALSE(status.ok());
//   EXPECT_THAT(status.error_message(), HasSubstr("Invalid node ID"));
//   EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
//   EXPECT_TRUE(IsInitialized());  // Initialized as we pushed config before
// }

// TEST_F(BfrtNodeTest, VerifyChassisConfigReportsRebootRequired) {
//   ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

//   ChassisConfig config;
//   config.add_nodes()->set_id(kNodeId);
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId + 1))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId + 1))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l2_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId + 1))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_l3_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId + 1))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId + 1))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId + 1))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_packetio_manager_mock_,
//               VerifyChassisConfig(EqualsProto(config), kNodeId + 1))
//       .WillOnce(Return(::util::OkStatus()));

//   ::util::Status status = VerifyChassisConfig(config, kNodeId + 1);
//   ASSERT_FALSE(status.ok());
//   EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
// }

TEST_F(BfrtNodeTest, ShutdownSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

  {
    InSequence sequence;  // The order of the calls are important. Enforce it.
    EXPECT_CALL(*bfrt_packetio_manager_mock_, Shutdown())
        .WillOnce(Return(::util::OkStatus()));
    // EXPECT_CALL(*bcm_tunnel_manager_mock_, Shutdown())
    //     .WillOnce(Return(::util::OkStatus()));
    // EXPECT_CALL(*bcm_acl_manager_mock_, Shutdown())
    //     .WillOnce(Return(::util::OkStatus()));
    // EXPECT_CALL(*bcm_l3_manager_mock_, Shutdown())
    //     .WillOnce(Return(::util::OkStatus()));
    // EXPECT_CALL(*bcm_l2_manager_mock_, Shutdown())
    //     .WillOnce(Return(::util::OkStatus()));
    // EXPECT_CALL(*bfrt_table_manager_mock_, Shutdown())
    //     .WillOnce(Return(::util::OkStatus()));
    // EXPECT_CALL(*p4_table_mapper_mock_, Shutdown())
    //     .WillOnce(Return(::util::OkStatus()));
  }

  EXPECT_OK(bfrt_node_->Shutdown());
  EXPECT_FALSE(IsInitialized());
  EXPECT_FALSE(IsPipelineInitialized());
}

TEST_F(BfrtNodeTest, ShutdownFailureWhenSomeManagerShutdownFails) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

  EXPECT_CALL(*bfrt_packetio_manager_mock_, Shutdown())
      .WillOnce(Return(DefaultError()));
  // EXPECT_CALL(*bcm_tunnel_manager_mock_, Shutdown())
  //     .WillOnce(Return(::util::OkStatus()));
  // EXPECT_CALL(*bcm_acl_manager_mock_, Shutdown())
  //     .WillOnce(Return(::util::OkStatus()));
  // EXPECT_CALL(*bcm_l3_manager_mock_, Shutdown())
  //     .WillOnce(Return(DefaultError()));
  // EXPECT_CALL(*bcm_l2_manager_mock_, Shutdown())
  //     .WillOnce(Return(::util::OkStatus()));
  // EXPECT_CALL(*bfrt_table_manager_mock_, Shutdown())
  //     .WillOnce(Return(::util::OkStatus()));
  // EXPECT_CALL(*p4_table_mapper_mock_, Shutdown())
  //     .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bfrt_node_->Shutdown(), DerivedFromStatus(DefaultError()));
}

// PushForwardingPipelineConfig() should verify and propagate the config.
TEST_F(BfrtNodeTest, PushForwardingPipelineConfigSuccess2) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());
}

// // PushForwardingPipelineConfig() should fail immediately on any push
// failures. TEST_F(BfrtNodeTest,
//        PushForwardingPipelineConfigFailueOnAnyManagerPushFailure) {
//   ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

//   ::p4::v1::ForwardingPipelineConfig config;
//   // Order matters here as if an earlier push fails, following pushes must
//   not
//   // be attempted.
//   EXPECT_CALL(*p4_table_mapper_mock_, HandlePrePushStaticEntryChanges(_, _))
//       .WillOnce(Return(DefaultError()))
//       .WillRepeatedly(Return(::util::OkStatus()));
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               PushForwardingPipelineConfig(EqualsProto(config)))
//       .WillOnce(Return(DefaultError()))
//       .WillRepeatedly(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               PushForwardingPipelineConfig(EqualsProto(config)))
//       .WillOnce(Return(DefaultError()))
//       .WillRepeatedly(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               PushForwardingPipelineConfig(EqualsProto(config)))
//       .WillOnce(Return(DefaultError()))
//       .WillRepeatedly(Return(::util::OkStatus()));
//   EXPECT_CALL(*p4_table_mapper_mock_, HandlePostPushStaticEntryChanges(_, _))
//       .WillOnce(Return(DefaultError()))
//       .WillRepeatedly(Return(::util::OkStatus()));

//   EXPECT_THAT(PushForwardingPipelineConfig(config),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_THAT(PushForwardingPipelineConfig(config),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_THAT(PushForwardingPipelineConfig(config),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_THAT(PushForwardingPipelineConfig(config),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_THAT(PushForwardingPipelineConfig(config),
//               DerivedFromStatus(DefaultError()));
// }

// VerifyForwardingPipelineConfig() should verify the config.
TEST_F(BfrtNodeTest, VerifyForwardingPipelineConfigSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

  ::p4::v1::ForwardingPipelineConfig config =
      GetDefaultForwardingPipelineConfig();
  {
    InSequence sequence;
    EXPECT_CALL(*bfrt_table_manager_mock_,
                VerifyForwardingPipelineConfig(EqualsProto(config)))
        .WillOnce(Return(::util::OkStatus()));
  }
  EXPECT_OK(VerifyForwardingPipelineConfig(config));
}

// // VerifyForwardingPipelineConfig() should fail immediately on any verify
// // failures.
// TEST_F(BfrtNodeTest,
//        VerifyForwardingPipelineConfigFailueOnAnyManagerVerifyFailure) {
//   ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

//   ::p4::v1::ForwardingPipelineConfig config;
//   EXPECT_CALL(*p4_table_mapper_mock_,
//               VerifyForwardingPipelineConfig(EqualsProto(config)))
//       .WillOnce(Return(DefaultError()))
//       .WillRepeatedly(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_acl_manager_mock_,
//               VerifyForwardingPipelineConfig(EqualsProto(config)))
//       .WillRepeatedly(Return(::util::OkStatus()));
//   EXPECT_CALL(*bcm_tunnel_manager_mock_,
//               VerifyForwardingPipelineConfig(EqualsProto(config)))
//       .WillRepeatedly(Return(::util::OkStatus()));

//   EXPECT_THAT(VerifyForwardingPipelineConfig(config),
//               DerivedFromStatus(DefaultError()));
//   EXPECT_OK(VerifyForwardingPipelineConfig(config));
//   EXPECT_OK(VerifyForwardingPipelineConfig(config));
//   EXPECT_OK(VerifyForwardingPipelineConfig(config));
// }

namespace {

::p4::v1::TableEntry* SetupTableEntryToInsert(::p4::v1::WriteRequest* req,
                                              uint64 node_id) {
  req->set_device_id(node_id);
  auto* update = req->add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  return entity->mutable_table_entry();
}

::p4::v1::TableEntry* SetupTableEntryToModify(::p4::v1::WriteRequest* req,
                                              uint64 node_id) {
  req->set_device_id(node_id);
  auto* update = req->add_updates();
  update->set_type(::p4::v1::Update::MODIFY);
  auto* entity = update->mutable_entity();
  return entity->mutable_table_entry();
}

::p4::v1::TableEntry* SetupTableEntryToDelete(::p4::v1::WriteRequest* req,
                                              uint64 node_id) {
  req->set_device_id(node_id);
  auto* update = req->add_updates();
  update->set_type(::p4::v1::Update::DELETE);
  auto* entity = update->mutable_entity();
  return entity->mutable_table_entry();
}

::p4::v1::TableEntry* SetupTableEntryToRead(::p4::v1::ReadRequest* req,
                                            uint64 node_id) {
  req->set_device_id(node_id);
  auto* entity = req->add_entities();
  return entity->mutable_table_entry();
}

}  // namespace

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_InsertTableEntry) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  auto* table_entry = SetupTableEntryToInsert(&req, kNodeId);

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteTableEntry(session_mock, ::p4::v1::Update::INSERT,
                              EqualsProto(*table_entry)))
      .WillOnce(Return(::util::OkStatus()));

  std::vector<::util::Status> results = {};
  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_ModifyTableEntry) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  auto* table_entry = SetupTableEntryToModify(&req, kNodeId);

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteTableEntry(session_mock, ::p4::v1::Update::MODIFY,
                              EqualsProto(*table_entry)))
      .WillOnce(Return(::util::OkStatus()));

  std::vector<::util::Status> results = {};
  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_DeleteTableEntry) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  auto* table_entry = SetupTableEntryToDelete(&req, kNodeId);

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteTableEntry(session_mock, ::p4::v1::Update::DELETE,
                              EqualsProto(*table_entry)))
      .WillOnce(Return(::util::OkStatus()));

  std::vector<::util::Status> results = {};
  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_InsertActionProfileMember) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  auto* member = entity->mutable_action_profile_member();
  member->set_member_id(kMemberId);
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteActionProfileMember(session_mock, ::p4::v1::Update::INSERT,
                                       EqualsProto(*member)))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_ModifyActionProfileMember) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::MODIFY);
  auto* entity = update->mutable_entity();
  auto* member = entity->mutable_action_profile_member();
  member->set_member_id(kMemberId);
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteActionProfileMember(session_mock, ::p4::v1::Update::MODIFY,
                                       EqualsProto(*member)))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_DeleteActionProfileMember) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::DELETE);
  auto* entity = update->mutable_entity();
  auto* member = entity->mutable_action_profile_member();
  member->set_member_id(kMemberId);
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteActionProfileMember(session_mock, ::p4::v1::Update::DELETE,
                                       EqualsProto(*member)))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_InsertActionProfileGroup) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  auto* group = entity->mutable_action_profile_group();
  group->set_group_id(kGroupId);
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteActionProfileGroup(session_mock, ::p4::v1::Update::INSERT,
                                      EqualsProto(*group)))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

// TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_InsertActionProfileGroup)
// {
//   ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

//   ::p4::v1::WriteRequest req;
//   req.set_device_id(kNodeId);
//   auto* update = req.add_updates();
//   update->set_type(::p4::v1::Update::INSERT);
//   auto* entity = update->mutable_entity();
//   auto* group = entity->mutable_action_profile_group();
//   group->set_group_id(kGroupId);
//   std::vector<::util::Status> results = {};

//   EXPECT_CALL(*bfrt_table_manager_mock_, ActionProfileGroupExists(kGroupId))
//       .WillOnce(Return(false));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               FillBcmMultipathNexthop(EqualsProto(*group), _))
//       .WillOnce(DoAll(WithArgs<1>(Invoke(
//                           [](BcmMultipathNexthop* x) {
//                           x->set_unit(kDeviceId); })),
//                       Return(::util::OkStatus())));
//   EXPECT_CALL(*bcm_l3_manager_mock_, FindOrCreateMultipathNexthop(_))
//       .WillOnce(Return(kEgressIntfId));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               AddActionProfileGroup(EqualsProto(*group), kEgressIntfId))
//       .WillOnce(Return(::util::OkStatus()));

//   EXPECT_OK(WriteForwardingEntries(req, &results));
//   EXPECT_EQ(1U, results.size());
// }

// TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_ModifyActionProfileGroup)
// {
//   ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

//   ::p4::v1::WriteRequest req;
//   req.set_device_id(kNodeId);
//   auto* update = req.add_updates();
//   update->set_type(::p4::v1::Update::MODIFY);
//   auto* entity = update->mutable_entity();
//   auto* group = entity->mutable_action_profile_group();
//   group->set_group_id(kGroupId);
//   std::vector<::util::Status> results = {};

//   EXPECT_CALL(*bfrt_table_manager_mock_, GetBcmMultipathNexthopInfo(kGroupId,
//   _))
//       .WillOnce(DoAll(WithArgs<1>(Invoke([](BcmMultipathNexthopInfo* x) {
//                         x->egress_intf_id = kEgressIntfId;
//                       })),
//                       Return(::util::OkStatus())));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               FillBcmMultipathNexthop(EqualsProto(*group), _))
//       .WillOnce(DoAll(WithArgs<1>(Invoke(
//                           [](BcmMultipathNexthop* x) {
//                           x->set_unit(kDeviceId); })),
//                       Return(::util::OkStatus())));
//   EXPECT_CALL(*bcm_l3_manager_mock_, ModifyMultipathNexthop(kEgressIntfId,
//   _))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               UpdateActionProfileGroup(EqualsProto(*group)))
//       .WillOnce(Return(::util::OkStatus()));

//   EXPECT_OK(WriteForwardingEntries(req, &results));
//   EXPECT_EQ(1U, results.size());
// }

// TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_DeleteActionProfileGroup)
// {
//   ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

//   ::p4::v1::WriteRequest req;
//   req.set_device_id(kNodeId);
//   auto* update = req.add_updates();
//   update->set_type(::p4::v1::Update::DELETE);
//   auto* entity = update->mutable_entity();
//   auto* group = entity->mutable_action_profile_group();
//   group->set_group_id(kGroupId);
//   std::vector<::util::Status> results = {};

//   EXPECT_CALL(*bfrt_table_manager_mock_, GetBcmMultipathNexthopInfo(kGroupId,
//   _))
//       .WillOnce(DoAll(WithArgs<1>(Invoke([](BcmMultipathNexthopInfo* x) {
//                         x->egress_intf_id = kEgressIntfId;
//                         x->flow_ref_count = 0;
//                       })),
//                       Return(::util::OkStatus())));
//   EXPECT_CALL(*bcm_l3_manager_mock_, DeleteMultipathNexthop(kEgressIntfId))
//       .WillOnce(Return(::util::OkStatus()));
//   EXPECT_CALL(*bfrt_table_manager_mock_,
//               DeleteActionProfileGroup(EqualsProto(*group)))
//       .WillOnce(Return(::util::OkStatus()));

//   EXPECT_OK(WriteForwardingEntries(req, &results));
//   EXPECT_EQ(1U, results.size());
// }

TEST_F(BfrtNodeTest,
       WriteForwardingEntriesSuccess_InsertExternEntryActionProfileMember) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  auto* extern_entry = entity->mutable_extern_entry();
  extern_entry->set_extern_type_id(kTnaExternActionProfileId);
  ::p4::v1::ActionProfileMember action_profile_member;
  extern_entry->mutable_entry()->PackFrom(action_profile_member);
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(
      *bfrt_table_manager_mock_,
      WriteActionProfileMember(session_mock, ::p4::v1::Update::INSERT, _))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest,
       WriteForwardingEntriesSuccess_InsertExternEntryActionProfileGroup) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  auto* extern_entry = entity->mutable_extern_entry();
  extern_entry->set_extern_type_id(kTnaExternActionSelectorId);
  ::p4::v1::ActionProfileGroup action_profile_group;
  extern_entry->mutable_entry()->PackFrom(action_profile_group);
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(
      *bfrt_table_manager_mock_,
      WriteActionProfileGroup(session_mock, ::p4::v1::Update::INSERT, _))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest,
       WriteForwardingEntriesSuccess_InsertPacketReplicationEngineEntry) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  auto* pre = entity->mutable_packet_replication_engine_entry();
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(
      *bfrt_pre_manager_mock_,
      WritePreEntry(session_mock, ::p4::v1::Update::INSERT, EqualsProto(*pre)))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_InsertDirectCounterEntry) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  auto* direct_counter_entry = entity->mutable_direct_counter_entry();
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteDirectCounterEntry(session_mock, ::p4::v1::Update::INSERT,
                                      EqualsProto(*direct_counter_entry)))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_InsertIndirectCounterEntry) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  auto* counter_entry = entity->mutable_counter_entry();
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_counter_manager_mock_,
              WriteIndirectCounterEntry(session_mock, ::p4::v1::Update::INSERT,
                                        EqualsProto(*counter_entry)))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_InsertRegisterEntry) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  auto* register_entry = entity->mutable_register_entry();
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteRegisterEntry(session_mock, ::p4::v1::Update::INSERT,
                                 EqualsProto(*register_entry)))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, WriteForwardingEntriesSuccess_InsertMeterEntry) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::WriteRequest req;
  req.set_device_id(kNodeId);
  auto* update = req.add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto* entity = update->mutable_entity();
  auto* meter_entry = entity->mutable_meter_entry();
  std::vector<::util::Status> results = {};

  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(*bfrt_table_manager_mock_,
              WriteMeterEntry(session_mock, ::p4::v1::Update::INSERT,
                              EqualsProto(*meter_entry)))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(WriteForwardingEntries(req, &results));
  EXPECT_EQ(1U, results.size());
}

TEST_F(BfrtNodeTest, ReadForwardingEntriesSuccess_TableEntry) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ASSERT_NO_FATAL_FAILURE(PushForwardingPipelineConfigWithCheck());

  ::p4::v1::ReadRequest req;
  auto* table_entry = SetupTableEntryToRead(&req, kNodeId);

  WriterMock<::p4::v1::ReadResponse> writer_mock;
  EXPECT_CALL(writer_mock, Write(_)).WillOnce(Return(true));
  std::shared_ptr<BfSdeInterface::SessionInterface> session_mock =
      std::make_shared<SessionMock>();
  EXPECT_CALL(*bf_sde_mock_, CreateSession()).WillOnce(Return(session_mock));
  EXPECT_CALL(
      *bfrt_table_manager_mock_,
      ReadTableEntry(session_mock, EqualsProto(*table_entry), &writer_mock))
      .WillOnce(Return(::util::OkStatus()));
  std::vector<::util::Status> results = {};
  EXPECT_OK(ReadForwardingEntries(req, &writer_mock, &results));
  EXPECT_EQ(1U, results.size());
}

// RegisterStreamMessageResponseWriter() should forward the call to
// BfrtPacketioManager and return success or error based on the returned result.
TEST_F(BfrtNodeTest, RegisterStreamMessageResponseWriter) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

  auto writer = std::make_shared<WriterMock<::p4::v1::StreamMessageResponse>>();
  EXPECT_CALL(*bfrt_packetio_manager_mock_, RegisterPacketReceiveWriter(_))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(DefaultError()));

  EXPECT_OK(RegisterStreamMessageResponseWriter(writer));
  EXPECT_THAT(RegisterStreamMessageResponseWriter(writer),
              DerivedFromStatus(DefaultError()));
}

// UnregisterStreamMessageResponseWriter() should forward the call to
// BfrtPacketioManager and return success or error based on the returned result.
TEST_F(BfrtNodeTest, UnregisterStreamMessageResponseWriter) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());

  EXPECT_CALL(*bfrt_packetio_manager_mock_, UnregisterPacketReceiveWriter())
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(DefaultError()));

  EXPECT_OK(UnregisterStreamMessageResponseWriter());
  EXPECT_THAT(UnregisterStreamMessageResponseWriter(),
              DerivedFromStatus(DefaultError()));
}

// HandleStreamMessageRequest() should forward the packet out to
// BfrtPacketioManager and return success or error based on the returned result.
TEST_F(BfrtNodeTest, HandleStreamMessageRequest_PacketOut) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ::p4::v1::StreamMessageRequest req;
  auto* packet = req.mutable_packet();

  EXPECT_CALL(*bfrt_packetio_manager_mock_,
              TransmitPacket(EqualsProto(*packet)))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(DefaultError()));

  EXPECT_OK(HandleStreamMessageRequest(req));
  EXPECT_THAT(HandleStreamMessageRequest(req),
              DerivedFromStatus(DefaultError()));
}

// HandleStreamMessageRequest() should reject invalid StreamMessageRequests.
TEST_F(BfrtNodeTest, HandleStreamMessageRequest_Invalid) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ::p4::v1::StreamMessageRequest req;

  EXPECT_THAT(HandleStreamMessageRequest(req),
              DerivedFromStatus(::util::Status(
                  StratumErrorSpace(), ERR_UNIMPLEMENTED, "Unsupported")));
}

// HandleStreamMessageRequest() should blindly accept StreamMessageRequests with
// digest acks.
TEST_F(BfrtNodeTest, HandleStreamMessageRequest_DigestAck) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ::p4::v1::StreamMessageRequest req;
  req.mutable_digest_ack();

  // TODO(max): extend once we actually implement digest acks.
  EXPECT_OK(HandleStreamMessageRequest(req));
}

// HandleStreamMessageRequest() should reject StreamMessageRequests with
// arbitrations.
TEST_F(BfrtNodeTest, HandleStreamMessageRequest_Arbitration) {
  ASSERT_NO_FATAL_FAILURE(PushChassisConfigWithCheck());
  ::p4::v1::StreamMessageRequest req;
  req.mutable_arbitration();

  EXPECT_THAT(HandleStreamMessageRequest(req),
              DerivedFromStatus(::util::Status(
                  StratumErrorSpace(), ERR_UNIMPLEMENTED, "Unsupported")));
}

// TODO(unknown): Complete unit test coverage.

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
