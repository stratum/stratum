// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/common/p4_service.h"

#include <memory>
#include <tuple>

#include "absl/memory/memory.h"
#include "absl/numeric/int128.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "google/rpc/code.pb.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/switch_mock.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/p4runtime/stream_message_reader_writer_mock.h"
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

DECLARE_int32(max_num_controllers_per_node);
DECLARE_int32(max_num_controller_connections);
DECLARE_string(forwarding_pipeline_configs_file);
DECLARE_string(write_req_log_file);
DECLARE_string(read_req_log_file);
DECLARE_string(test_tmpdir);

namespace stratum {
namespace hal {

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithArgs;

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

typedef ::grpc::ClientReaderWriter<::p4::v1::StreamMessageRequest,
                                   ::p4::v1::StreamMessageResponse>
    ClientStreamChannelReaderWriter;

class P4ServiceTest
    : public ::testing::TestWithParam<std::tuple<OperationMode, bool>> {
 protected:
  void SetUp() override {
    mode_ = ::testing::get<0>(GetParam());
    role_name_ = ::testing::get<1>(GetParam()) ? kRoleName1 : "";
    switch_mock_ = absl::make_unique<SwitchMock>();
    auth_policy_checker_mock_ = absl::make_unique<AuthPolicyCheckerMock>();
    error_buffer_ = absl::make_unique<ErrorBuffer>();
    p4_service_ = absl::make_unique<P4Service>(mode_, switch_mock_.get(),
                                               auth_policy_checker_mock_.get(),
                                               error_buffer_.get());
    std::string url =
        "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(url, ::grpc::InsecureServerCredentials());
    builder.RegisterService(p4_service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);
    stub_ = ::p4::v1::P4Runtime::NewStub(
        ::grpc::CreateChannel(url, ::grpc::InsecureChannelCredentials()));
    ASSERT_NE(stub_, nullptr);
    FLAGS_max_num_controllers_per_node = 5;
    FLAGS_max_num_controller_connections = 20;
    FLAGS_forwarding_pipeline_configs_file =
        FLAGS_test_tmpdir + "/forwarding_pipeline_configs_file.pb.txt";
    FLAGS_write_req_log_file = FLAGS_test_tmpdir + "/write_req_log_fil.csv";
    FLAGS_read_req_log_file = FLAGS_test_tmpdir + "/read_req_log_fil.csv";
    // Before starting the tests, remove the read and write req file if exists.
    if (PathExists(FLAGS_write_req_log_file)) {
      ASSERT_OK(RemoveFile(FLAGS_write_req_log_file));
    }
    if (PathExists(FLAGS_read_req_log_file)) {
      ASSERT_OK(RemoveFile(FLAGS_read_req_log_file));
    }
  }

  void TearDown() override { server_->Shutdown(); }

  void OnPacketReceive(const ::p4::v1::PacketIn& packet) {
    ::p4::v1::StreamMessageResponse resp;
    *resp.mutable_packet() = packet;
    p4_service_->StreamResponseReceiveHandler(kNodeId1, resp);
  }

  void OnDigestListReceive(const ::p4::v1::DigestList& digest) {
    ::p4::v1::StreamMessageResponse resp;
    *resp.mutable_digest() = digest;
    p4_service_->StreamResponseReceiveHandler(kNodeId1, resp);
  }

  void FillTestForwardingPipelineConfigsAndSave(
      ForwardingPipelineConfigs* configs) {
    const std::string& configs_text = absl::Substitute(
        kForwardingPipelineConfigsTemplate, kNodeId1, kNodeId2);
    ASSERT_OK(ParseProtoFromString(configs_text, configs));
    ASSERT_OK(WriteStringToFile(configs_text,
                                FLAGS_forwarding_pipeline_configs_file));
  }

  void CheckForwardingPipelineConfigs(const ForwardingPipelineConfigs* configs,
                                      uint64 node_id) {
    absl::ReaderMutexLock l(&p4_service_->config_lock_);
    if (configs == nullptr) {
      ASSERT_TRUE(p4_service_->forwarding_pipeline_configs_ == nullptr);
    } else {
      ASSERT_TRUE(p4_service_->forwarding_pipeline_configs_ != nullptr);
      EXPECT_TRUE(ProtoEqual(
          configs->node_id_to_config().at(node_id),
          p4_service_->forwarding_pipeline_configs_->node_id_to_config().at(
              node_id)));
    }
  }

  void SetTestForwardingPipelineConfigs() {
    absl::WriterMutexLock l(&p4_service_->config_lock_);
    ASSERT_TRUE(p4_service_->forwarding_pipeline_configs_ == nullptr);
    p4_service_->forwarding_pipeline_configs_ =
        absl::make_unique<ForwardingPipelineConfigs>();
    const std::string& configs_text = absl::Substitute(
        kForwardingPipelineConfigsTemplate, kNodeId1, kNodeId2);
    ASSERT_OK(ParseProtoFromString(
        configs_text, p4_service_->forwarding_pipeline_configs_.get()));
  }

  void AddFakeMasterController(
      uint64 node_id, p4runtime::SdnConnection* controller,
      const P4RoleConfig& role_config = GetRoleConfig()) {
    p4::v1::MasterArbitrationUpdate request;
    request.set_device_id(node_id);
    request.mutable_election_id()->set_high(
        absl::Uint128High64(controller->GetElectionId().value()));
    request.mutable_election_id()->set_low(
        absl::Uint128Low64(controller->GetElectionId().value()));
    if (!role_name_.empty()) {
      request.mutable_role()->set_name(role_name_);
      ASSERT_TRUE(
          request.mutable_role()->mutable_config()->PackFrom(role_config));
    }
    ASSERT_OK(p4_service_->AddOrModifyController(node_id, request, controller));
  }

  int GetNumberOfActiveConnections(uint64 node_id) {
    absl::WriterMutexLock l(&p4_service_->controller_lock_);
    if (!p4_service_->node_id_to_controller_manager_.count(node_id)) return 0;
    return p4_service_->node_id_to_controller_manager_.at(node_id)
        .ActiveConnections();
  }

  int GetNumberOfConnections() {
    absl::WriterMutexLock l(&p4_service_->controller_lock_);
    return p4_service_->num_controller_connections_;
  }

  static P4RoleConfig GetRoleConfig() {
    P4RoleConfig role_config;
    CHECK_OK(ParseProtoFromString(kRoleConfigText, &role_config));
    return role_config;
  }

  static constexpr char kForwardingPipelineConfigsTemplate[] = R"(
      node_id_to_config {
        key: $0
        value {
          p4info {
            tables {
              preamble {
                name: "some_table"
                id: 12  # kTableId1
              }
            }
            meters {
              preamble {
                name: "some_meter"
                id: 641
              }
            }
            registers {
              preamble {
                name: "some_register"
                id: 267
              }
            }
            counters {
              preamble {
                name: "some_counter"
                id: 719
              }
            }
            controller_packet_metadata {
              metadata {
                id: 666666
                name: "some_metadata_field"
                bitwidth: 16
              }
            }
          }
          p4_device_config: "\x01\x02\x03\x04\x05"
        }
      }
      node_id_to_config {
        key: $1
        value {
          p4info {
            tables {
              preamble {
                name: "another_table"
              }
            }
          }
          p4_device_config: "\x05\x04\x03\x02\x01"
        }
      }
  )";
  static constexpr char kTestPacketMetadata1[] = R"(
      metadata_id: 123456
      value: "\x00\x01"
  )";
  static constexpr char kTestPacketMetadata2[] = R"(
      metadata_id: 654321
      value: "\x12"
  )";
  static constexpr char kTestPacketMetadata3[] = R"(
      metadata_id: 666666
      value: "\x12"
  )";
  static constexpr char kTestPacketMetadata4[] = R"(
  )";
  static constexpr char kTestPacketMetadata5[] = R"(
      metadata_id: 123456
      value: "\x12"
  )";
  static constexpr char kTestDigestList1[] = R"(
      digest_id: 123456
      list_id: 654321
      timestamp: 1234567890
  )";
  static constexpr char kTestDigestListAck1[] = R"(
      digest_id: 123456
      list_id: 654321
  )";
  static constexpr char kRoleConfigText[] = R"pb(
      exclusive_p4_ids: 12  # kTableId1
      exclusive_p4_ids: 641
      exclusive_p4_ids: 267
      exclusive_p4_ids: 719
      packet_in_filter {
        metadata_id: 666666
        value: "\x12"
      }
      receives_packet_ins: true
      can_push_pipeline: true
  )pb";
  static constexpr char kRoleName1[] = "TestRole1";
  static constexpr char kRoleName2[] = "TestRole2";
  static constexpr char kOperErrorMsg[] = "Some error";
  static constexpr char kAggrErrorMsg[] = "A few errors happened";
  static constexpr uint64 kNodeId1 = 123123123;
  static constexpr uint64 kNodeId2 = 456456456;
  // The relative values of kElectionIdx constants are important. The highest
  // election ID at any time will determine the master.
  static constexpr absl::uint128 kElectionId1 = 1111;
  static constexpr absl::uint128 kElectionId2 = 2222;
  static constexpr absl::uint128 kElectionId3 = 1212;
  static constexpr uint32 kTableId1 = 12;
  static constexpr uint64 kCookie1 = 123;
  static constexpr uint64 kCookie2 = 321;
  OperationMode mode_;
  std::string role_name_;
  std::unique_ptr<SwitchMock> switch_mock_;
  std::unique_ptr<AuthPolicyCheckerMock> auth_policy_checker_mock_;
  std::unique_ptr<ErrorBuffer> error_buffer_;
  std::unique_ptr<P4Service> p4_service_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<::p4::v1::P4Runtime::Stub> stub_;
};

constexpr char P4ServiceTest::kForwardingPipelineConfigsTemplate[];
constexpr char P4ServiceTest::kTestPacketMetadata1[];
constexpr char P4ServiceTest::kTestPacketMetadata2[];
constexpr char P4ServiceTest::kTestPacketMetadata3[];
constexpr char P4ServiceTest::kTestPacketMetadata4[];
constexpr char P4ServiceTest::kTestPacketMetadata5[];
constexpr char P4ServiceTest::kTestDigestList1[];
constexpr char P4ServiceTest::kTestDigestListAck1[];
constexpr char P4ServiceTest::kRoleConfigText[];
constexpr char P4ServiceTest::kRoleName1[];
constexpr char P4ServiceTest::kRoleName2[];
constexpr char P4ServiceTest::kOperErrorMsg[];
constexpr char P4ServiceTest::kAggrErrorMsg[];
constexpr uint64 P4ServiceTest::kNodeId1;
constexpr uint64 P4ServiceTest::kNodeId2;
constexpr uint64 P4ServiceTest::kCookie1;
constexpr uint64 P4ServiceTest::kCookie2;
constexpr absl::uint128 P4ServiceTest::kElectionId1;
constexpr absl::uint128 P4ServiceTest::kElectionId2;
constexpr absl::uint128 P4ServiceTest::kElectionId3;
constexpr uint32 P4ServiceTest::kTableId1;

TEST_P(P4ServiceTest, ColdbootSetupSuccessForSavedConfigs) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Setup the test config and also save it to the file.
  ForwardingPipelineConfigs configs;
  FillTestForwardingPipelineConfigsAndSave(&configs);

  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId1, EqualsProto(configs.node_id_to_config().at(kNodeId1))))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId2, EqualsProto(configs.node_id_to_config().at(kNodeId2))))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  ASSERT_OK(p4_service_->Setup(false));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());
  CheckForwardingPipelineConfigs(&configs, kNodeId1);
  CheckForwardingPipelineConfigs(&configs, kNodeId2);
}

TEST_P(P4ServiceTest, ColdbootSetupSuccessForNoSavedConfig) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Delete the saved config. There will be no config push.
  if (PathExists(FLAGS_forwarding_pipeline_configs_file)) {
    ASSERT_OK(RemoveFile(FLAGS_forwarding_pipeline_configs_file));
  }

  // Call and validate results.
  ASSERT_OK(p4_service_->Setup(false));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

TEST_P(P4ServiceTest, ColdbootSetupFailureWhenPushFailsForSomeNodes) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Setup the test config and also save it to the file.
  ForwardingPipelineConfigs configs;
  FillTestForwardingPipelineConfigsAndSave(&configs);

  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId1, EqualsProto(configs.node_id_to_config().at(kNodeId1))))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kOperErrorMsg)));
  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId2, EqualsProto(configs.node_id_to_config().at(kNodeId2))))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  ::util::Status status = p4_service_->Setup(false);
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kOperErrorMsg));
  const auto& errors = error_buffer_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kOperErrorMsg));
  EXPECT_THAT(errors[0].error_message(),
              HasSubstr("saved forwarding pipeline configs"));
  CheckForwardingPipelineConfigs(&configs, kNodeId2);
}

TEST_P(P4ServiceTest, WarmbootSetupSuccessForSavedConfig) {
  // Setup the test config and also save it to the file. In case of warmboot
  // we read the file but we dont push anything to hardware.
  ForwardingPipelineConfigs configs;
  FillTestForwardingPipelineConfigsAndSave(&configs);

  // Call and validate results.
  ASSERT_OK(p4_service_->Setup(true));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());
  CheckForwardingPipelineConfigs(&configs, kNodeId1);
  CheckForwardingPipelineConfigs(&configs, kNodeId2);
}

TEST_P(P4ServiceTest, WarmbootSetupFailureForNoSavedConfig) {
  // Delete the saved config. There will be no config push.
  if (PathExists(FLAGS_forwarding_pipeline_configs_file)) {
    ASSERT_OK(RemoveFile(FLAGS_forwarding_pipeline_configs_file));
  }

  // Call and validate results.
  ::util::Status status = p4_service_->Setup(true);
  ASSERT_EQ(ERR_FILE_NOT_FOUND, status.error_code());
  const auto& errors = error_buffer_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(),
              HasSubstr("not read the saved forwarding pipeline configs"));
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

TEST_P(P4ServiceTest, WarmbootSetupFailureForBadSavedConfig) {
  // Write some invalid data to FLAGS_forwarding_pipeline_configs_file so that
  // the parsing fails.
  ASSERT_OK(
      WriteStringToFile("blah blah", FLAGS_forwarding_pipeline_configs_file));

  // Call and validate results.
  ::util::Status status = p4_service_->Setup(true);
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  const auto& errors = error_buffer_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(),
              HasSubstr("not read the saved forwarding pipeline configs"));
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

TEST_P(P4ServiceTest, SetupAndThenTeardownSuccess) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  ForwardingPipelineConfigs configs;
  FillTestForwardingPipelineConfigsAndSave(&configs);

  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId1, EqualsProto(configs.node_id_to_config().at(kNodeId1))))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId2, EqualsProto(configs.node_id_to_config().at(kNodeId2))))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  ASSERT_OK(p4_service_->Setup(false));
  CheckForwardingPipelineConfigs(&configs, kNodeId1);
  CheckForwardingPipelineConfigs(&configs, kNodeId2);
  ASSERT_OK(p4_service_->Teardown());
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

// Pushing a different forwarding pipeline config again should work.
TEST_P(P4ServiceTest, SetupAndPushForwardingPipelineConfigSuccess) {
  ForwardingPipelineConfigs configs;
  FillTestForwardingPipelineConfigsAndSave(&configs);

  EXPECT_CALL(*switch_mock_, PushForwardingPipelineConfig(_, _))
      .WillRepeatedly(Return(::util::OkStatus()));

  // Call and validate results.
  ASSERT_OK(p4_service_->Setup(false));
  if (mode_ == OPERATION_MODE_COUPLED) {
    // In the coupled mode and coldboot, we do nothing.
    CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
  } else {
    // In other modes, config is pushed.
    CheckForwardingPipelineConfigs(&configs, kNodeId1);
    CheckForwardingPipelineConfigs(&configs, kNodeId2);
  }

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));

  ::grpc::ServerContext context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);

  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_role(role_name_);
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  configs.mutable_node_id_to_config()->at(kNodeId1).set_p4_device_config(
      "fake");  // emulate a modification in the config
  *request.mutable_config() = configs.node_id_to_config().at(kNodeId1);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_TRUE(status.ok()) << "Error: " << status.error_message();
  CheckForwardingPipelineConfigs(&configs, kNodeId1);
  if (mode_ != OPERATION_MODE_COUPLED) {
    CheckForwardingPipelineConfigs(&configs, kNodeId2);
  }
  ASSERT_OK(p4_service_->Teardown());
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

TEST_P(P4ServiceTest, VerifyForwardingPipelineConfigSuccess) {
  ForwardingPipelineConfigs configs;
  FillTestForwardingPipelineConfigsAndSave(&configs);

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, VerifyForwardingPipelineConfig(_, _))
      .WillRepeatedly(Return(::util::OkStatus()));

  ::grpc::ServerContext context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_role(role_name_);
  request.set_action(::p4::v1::SetForwardingPipelineConfigRequest::VERIFY);
  *request.mutable_config() = configs.node_id_to_config().at(kNodeId1);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_TRUE(status.ok()) << "Error: " << status.error_message();
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

TEST_P(P4ServiceTest, VerifyForwardingPipelineConfigFailureForNonMaster) {
  ForwardingPipelineConfigs configs;
  FillTestForwardingPipelineConfigsAndSave(&configs);

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, VerifyForwardingPipelineConfig(_, _))
      .WillRepeatedly(Return(::util::OkStatus()));

  ::grpc::ServerContext context;
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_action(::p4::v1::SetForwardingPipelineConfigRequest::VERIFY);
  *request.mutable_config() = configs.node_id_to_config().at(kNodeId1);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("from non-master is not permitted for node"));
}

TEST_P(P4ServiceTest, SetForwardingPipelineConfigFailureForAuthError) {
  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kAggrErrorMsg)));

  ::grpc::ServerContext context;
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr(kAggrErrorMsg));
}

TEST_P(P4ServiceTest, SetForwardingPipelineConfigFailureForNoNodeId) {
  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));

  ::grpc::ServerContext context;
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid device ID."));
}

TEST_P(P4ServiceTest, SetForwardingPipelineConfigFailureForNoElectionId) {
  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));

  ::grpc::ServerContext context;
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid election ID for node"));
}

TEST_P(P4ServiceTest, SetForwardingPipelineConfigFailureForNonMaster) {
  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));

  ::grpc::ServerContext context;
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("from non-master is not permitted for node"));
}

TEST_P(P4ServiceTest, PushForwardingPipelineConfigFailureWhenPushFails) {
  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_CALL(*switch_mock_, PushForwardingPipelineConfig(kNodeId1, _))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kAggrErrorMsg)));

  ::grpc::ServerContext context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_role(role_name_);
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr(kAggrErrorMsg));
}

TEST_P(P4ServiceTest, PushForwardingPipelineConfigReportsRebootRequired) {
  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_CALL(*switch_mock_, PushForwardingPipelineConfig(kNodeId1, _))
      .WillOnce(Return(::util::Status(StratumErrorSpace(), ERR_REBOOT_REQUIRED,
                                      "reboot required")));

  ::grpc::ServerContext context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_role(role_name_);
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("reboot required"));

  // CheckForwardingPipelineConfigs(&configs, kNodeId1);
  ASSERT_OK(p4_service_->Teardown());
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

TEST_P(P4ServiceTest, SetForwardingPipelineConfigFailureForRoleProhibited) {
  // This test is specific to role configs.
  if (role_name_.empty()) {
    GTEST_SKIP();
  }
  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));

  constexpr char kRoleConfigText[] = R"pb(
      can_push_pipeline: false
  )pb";
  P4RoleConfig role_config;
  CHECK_OK(ParseProtoFromString(kRoleConfigText, &role_config));
  ::grpc::ServerContext context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller, role_config);

  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_role(role_name_);
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("not allowed to push pipelines"));

  ASSERT_OK(p4_service_->Teardown());
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

TEST_P(P4ServiceTest, WriteSuccess) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ServerContext server_context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&server_context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);

  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.set_device_id(kNodeId1);
  req.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  req.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  req.set_role(role_name_);
  req.add_updates()->set_type(::p4::v1::Update::INSERT);
  req.mutable_updates(0)->mutable_entity()->mutable_table_entry()->set_table_id(
      kTableId1);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(::util::OkStatus()));
  const std::vector<::util::Status> kExpectedResults = {::util::OkStatus()};
  EXPECT_CALL(*switch_mock_, WriteForwardingEntries(EqualsProto(req), _))
      .WillOnce(DoAll(SetArgPointee<1>(kExpectedResults),
                      Return(::util::OkStatus())));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(status.error_message().empty());
  EXPECT_TRUE(status.error_details().empty());
  std::string s;
  ASSERT_OK(ReadFileToString(FLAGS_write_req_log_file, &s));
  EXPECT_THAT(s, HasSubstr(req.updates(0).ShortDebugString()));
}

TEST_P(P4ServiceTest, WriteSuccessForNoUpdatesToWrite) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(status.error_message().empty());
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, WriteFailureForNoDeviceId) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.add_updates()->set_type(::p4::v1::Update::INSERT);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid device ID"));
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, WriteFailureForNoElectionId) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.set_device_id(kNodeId1);
  req.add_updates()->set_type(::p4::v1::Update::INSERT);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid election ID"));
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, WriteFailureWhenNonMaster) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.set_device_id(kNodeId1);
  req.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  req.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  req.add_updates()->set_type(::p4::v1::Update::INSERT);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("not permitted"));
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, WriteFailureWhenWriteForwardingEntriesFails) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ServerContext server_context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&server_context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);
  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.set_device_id(kNodeId1);
  req.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  req.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  req.set_role(role_name_);
  req.add_updates()->set_type(::p4::v1::Update::INSERT);
  req.mutable_updates(0)->mutable_entity()->mutable_table_entry()->set_table_id(
      kTableId1);
  req.add_updates()->set_type(::p4::v1::Update::MODIFY);
  req.mutable_updates(1)->mutable_entity()->mutable_table_entry()->set_table_id(
      kTableId1);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(::util::OkStatus()));
  const std::vector<::util::Status> kExpectedResults = {
      ::util::OkStatus(),
      ::util::Status(StratumErrorSpace(), ERR_TABLE_FULL, kOperErrorMsg)};
  EXPECT_CALL(*switch_mock_, WriteForwardingEntries(EqualsProto(req), _))
      .WillOnce(DoAll(
          SetArgPointee<1>(kExpectedResults),
          Return(::util::Status(StratumErrorSpace(),
                                ERR_AT_LEAST_ONE_OPER_FAILED, kAggrErrorMsg))));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr(kAggrErrorMsg));
  ::google::rpc::Status details;
  ASSERT_TRUE(details.ParseFromString(status.error_details()));
  ASSERT_EQ(2, details.details_size());
  ::p4::v1::Error detail;
  ASSERT_TRUE(details.details(0).UnpackTo(&detail));
  EXPECT_EQ(::google::rpc::OK, detail.canonical_code());
  ASSERT_TRUE(details.details(1).UnpackTo(&detail));
  EXPECT_EQ(::google::rpc::OUT_OF_RANGE, detail.canonical_code());
  EXPECT_EQ(kOperErrorMsg, detail.message());
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());
  std::string s;
  ASSERT_OK(ReadFileToString(FLAGS_write_req_log_file, &s));
  EXPECT_THAT(s, HasSubstr(req.updates(0).ShortDebugString()));
  EXPECT_THAT(s, HasSubstr(req.updates(1).ShortDebugString()));
}

TEST_P(P4ServiceTest, WriteFailureForAuthError) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.add_updates()->set_type(::p4::v1::Update::INSERT);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kAggrErrorMsg)));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr(kAggrErrorMsg));
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, WriteFailureWhenSwitchNotInitializedError) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ServerContext server_context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&server_context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);
  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.set_device_id(kNodeId1);
  req.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  req.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  req.set_role(role_name_);
  req.add_updates()->set_type(::p4::v1::Update::INSERT);
  req.mutable_updates(0)->mutable_entity()->mutable_table_entry()->set_table_id(
      kTableId1);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(::util::OkStatus()));
  const std::vector<::util::Status> kExpectedResults = {};
  EXPECT_CALL(*switch_mock_, WriteForwardingEntries(EqualsProto(req), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(kExpectedResults),
                Return(::util::Status(StratumErrorSpace(), ERR_NOT_INITIALIZED,
                                      kAggrErrorMsg))));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(::grpc::StatusCode::FAILED_PRECONDITION, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kAggrErrorMsg));
  // TODO(max): P4Runtime spec says error_details should be empty for failures
  // not related to the supplied flow entries.
  // EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, WriteFailureForNoPipeline) {
  // Not setting a pipeline here.
  ::grpc::ServerContext server_context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&server_context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);

  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.set_device_id(kNodeId1);
  req.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  req.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  req.set_role(role_name_);
  req.add_updates()->set_type(::p4::v1::Update::INSERT);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_FAILED_PRECONDITION, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("No valid forwarding pipeline"));
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, WriteFailureForWritingOutsideRoleAllowedTable) {
  // This test is specific to role configs.
  if (role_name_.empty()) {
    GTEST_SKIP();
  }

  SetTestForwardingPipelineConfigs();
  ::grpc::ServerContext server_context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&server_context, &stream);
  controller.SetElectionId(kElectionId1);
  controller.SetRoleName(kRoleName1);
  AddFakeMasterController(kNodeId1, &controller);

  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.set_device_id(kNodeId1);
  req.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  req.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  req.set_role(role_name_);
  req.add_updates()->set_type(::p4::v1::Update::INSERT);
  req.mutable_updates(0)->mutable_entity()->mutable_table_entry()->set_table_id(
      1234);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Write", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Write(&context, req, &resp);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(::grpc::StatusCode::PERMISSION_DENIED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("is not allowed to access entity"));
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, ReadSuccess) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
  req.set_role(role_name_);
  req.add_entities()->mutable_table_entry()->set_table_id(kTableId1);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(::util::OkStatus()));
  const std::vector<::util::Status> kExpectedResults = {::util::OkStatus()};
  EXPECT_CALL(*switch_mock_, ReadForwardingEntries(EqualsProto(req), _, _))
      .WillOnce(DoAll(SetArgPointee<2>(kExpectedResults),
                      Return(::util::OkStatus())));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_TRUE(status.ok());
  std::string s;
  ASSERT_OK(ReadFileToString(FLAGS_read_req_log_file, &s));
  EXPECT_THAT(s, HasSubstr(req.entities(0).ShortDebugString()));
}

TEST_P(P4ServiceTest, ReadSuccessForNoEntitiesToRead) {
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
  req.set_role(role_name_);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_TRUE(status.ok());
}

TEST_P(P4ServiceTest, ReadSuccessForRoleWildcardExpansion) {
  SetTestForwardingPipelineConfigs();

  ::grpc::ServerContext server_context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&server_context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);

  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
  req.set_role(role_name_);
  req.add_entities()->mutable_table_entry()->set_table_id(0);        // Wildcard
  req.add_entities()->mutable_register_entry()->set_register_id(0);  // Wildcard
  req.add_entities()->mutable_meter_entry()->set_meter_id(0);        // Wildcard
  req.add_entities()->mutable_counter_entry()->set_counter_id(0);    // Wildcard

  ::p4::v1::ReadRequest expected_req = req;
  if (!role_name_.empty()) {
    expected_req.mutable_entities(0)->mutable_table_entry()->set_table_id(
        kTableId1);
    expected_req.mutable_entities(1)->mutable_register_entry()->set_register_id(
        267);
    expected_req.mutable_entities(2)->mutable_meter_entry()->set_meter_id(641);
    expected_req.mutable_entities(3)->mutable_counter_entry()->set_counter_id(
        719);
  }

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(::util::OkStatus()));
  const std::vector<::util::Status> kExpectedResults = {
      ::util::OkStatus(), ::util::OkStatus(), ::util::OkStatus(),
      ::util::OkStatus()};
  EXPECT_CALL(*switch_mock_,
              ReadForwardingEntries(EqualsProto(expected_req), _, _))
      .WillOnce(DoAll(SetArgPointee<2>(kExpectedResults),
                      Return(::util::OkStatus())));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_TRUE(status.ok());
  std::string s;
  ASSERT_OK(ReadFileToString(FLAGS_read_req_log_file, &s));
  EXPECT_THAT(s, HasSubstr(req.entities(0).ShortDebugString()));
}

TEST_P(P4ServiceTest, ReadFailureForNoDeviceId) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_role(role_name_);
  req.add_entities()->mutable_table_entry()->set_table_id(kTableId1);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid device ID"));
}

TEST_P(P4ServiceTest, ReadFailureWhenReadForwardingEntriesFails) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
  req.set_role(role_name_);
  req.add_entities()->mutable_table_entry()->set_table_id(kTableId1);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(::util::OkStatus()));

  const std::vector<::util::Status> kExpectedDetails = {
      ::util::Status(StratumErrorSpace(), ERR_TABLE_FULL, kOperErrorMsg)};
  EXPECT_CALL(*switch_mock_, ReadForwardingEntries(EqualsProto(req), _, _))
      .WillOnce(DoAll(SetArgPointee<2>(kExpectedDetails),
                      Return(::util::Status(StratumErrorSpace(), ERR_INTERNAL,
                                            kAggrErrorMsg))));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr(kAggrErrorMsg));
  ::google::rpc::Status details;
  ASSERT_TRUE(details.ParseFromString(status.error_details()));
  ASSERT_EQ(1, details.details_size());
  ::p4::v1::Error detail;
  ASSERT_TRUE(details.details(0).UnpackTo(&detail));
  EXPECT_EQ(::google::rpc::OUT_OF_RANGE, detail.canonical_code());
  EXPECT_EQ(kOperErrorMsg, detail.message());
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());
  std::string s;
  ASSERT_OK(ReadFileToString(FLAGS_read_req_log_file, &s));
  EXPECT_THAT(s, HasSubstr(req.entities(0).ShortDebugString()));
}

TEST_P(P4ServiceTest, ReadFailureForAuthError) {
  SetTestForwardingPipelineConfigs();
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
  req.set_role(role_name_);
  req.add_entities()->mutable_table_entry()->set_table_id(kTableId1);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kAggrErrorMsg)));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr(kAggrErrorMsg));
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, ReadFailureForNoPipeline) {
  // Not setting a pipeline here.
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
  req.set_role(role_name_);
  req.add_entities()->mutable_table_entry()->set_table_id(kTableId1);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_FAILED_PRECONDITION, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("No valid forwarding pipeline"));
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, ReadFailureForRoleProhibited) {
  // This test is specific to role configs.
  if (role_name_.empty()) {
    GTEST_SKIP();
  }

  SetTestForwardingPipelineConfigs();
  ::grpc::ServerContext server_context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&server_context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);

  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
  req.set_role(role_name_);
  req.add_entities()->mutable_table_entry()->set_table_id(1234);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_PERMISSION_DENIED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("is not allowed to access entity"));
  EXPECT_TRUE(status.error_details().empty());
}

// This test cannot be really broken down to multiple tests as it tries to test
// a sequence of events. To make the debugging simpler, we use ASSERT_XXX to
// stop executing as soon as an error happens as the rest of the test might get
// to an unknown state.
TEST_P(P4ServiceTest, StreamChannelSuccess) {
  ::grpc::ClientContext context1;
  ::grpc::ClientContext context2;
  ::grpc::ClientContext context3;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  // Sample role config.
  const P4RoleConfig role_config = GetRoleConfig();

  // Sample packets. We dont care about payload.
  ::p4::v1::PacketOut packet1;
  ::p4::v1::PacketOut packet2;
  ::p4::v1::PacketIn packet3;
  ::p4::v1::PacketOut packet4;
  ::p4::v1::PacketIn packet5;
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata1, packet1.add_metadata()));
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata2, packet2.add_metadata()));
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata3, packet3.add_metadata()));
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata4, packet4.add_metadata()));
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata5, packet5.add_metadata()));

  // Sample digest lists and acks. We don't care about data.
  ::p4::v1::DigestList digest_list1;
  ::p4::v1::DigestListAck digest_ack1;
  ASSERT_OK(ParseProtoFromString(kTestDigestList1, &digest_list1));
  ASSERT_OK(ParseProtoFromString(kTestDigestListAck1, &digest_ack1));

  // Sample StreamMessageRequests.
  ::p4::v1::StreamMessageRequest req1;
  ::p4::v1::StreamMessageRequest req2;
  ::p4::v1::StreamMessageRequest req3;
  *req1.mutable_packet() = packet2;
  *req2.mutable_packet() = packet4;
  *req3.mutable_digest_ack() = digest_ack1;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_,
              HandleStreamMessageRequest(kNodeId1, EqualsProto(req1)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_,
              HandleStreamMessageRequest(kNodeId1, EqualsProto(req2)))
      .WillOnce(Return(::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                                      kOperErrorMsg)));
  EXPECT_CALL(*switch_mock_,
              HandleStreamMessageRequest(kNodeId1, EqualsProto(req3)))
      .WillOnce(Return(::util::OkStatus()));

  //----------------------------------------------------------------------------
  // Before any connection, any PacketIn received from the CPU will be
  // ignored.
  OnPacketReceive(packet3);

  //----------------------------------------------------------------------------
  // Before any connection, any digest list received from the switch will be
  // ignored.
  OnDigestListReceive(digest_list1);

  //----------------------------------------------------------------------------
  // Now start with making the stream channels for all the controllers. We use
  // 3 streams to emulate 3 controllers.
  std::unique_ptr<ClientStreamChannelReaderWriter> stream1 =
      stub_->StreamChannel(&context1);
  std::unique_ptr<ClientStreamChannelReaderWriter> stream2 =
      stub_->StreamChannel(&context2);
  std::unique_ptr<ClientStreamChannelReaderWriter> stream3 =
      stub_->StreamChannel(&context3);

  //----------------------------------------------------------------------------
  // Controller #1 connect and becomes master.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  if (!role_name_.empty()) {
    req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
    req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
        role_config);
  }
  ASSERT_TRUE(stream1->Write(req));

  // Read the mastership info back.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId1),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId1),
            resp.arbitration().election_id().low());
  if (!role_name_.empty()) {
    ASSERT_EQ(kRoleName1, resp.arbitration().role().name());
    P4RoleConfig returned_role_config;
    ASSERT_TRUE(
        resp.arbitration().role().config().UnpackTo(&returned_role_config));
    ASSERT_THAT(role_config, EqualsProto(returned_role_config));
  }
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());
  ASSERT_EQ(1, GetNumberOfActiveConnections(kNodeId1));

  //----------------------------------------------------------------------------
  // Controller #2 connects and since it has higher election_id it becomes the
  // new master.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId2));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId2));
  if (!role_name_.empty()) {
    req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
    req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
        role_config);
  }
  ASSERT_TRUE(stream2->Write(req));

  // Read the mastership info back. It will be sent to Controller #1 and #2.
  // Status will be OK for Controller #2 and non-OK for Controller #1.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  if (!role_name_.empty()) {
    ASSERT_EQ(kRoleName1, resp.arbitration().role().name());
    P4RoleConfig returned_role_config;
    ASSERT_TRUE(
        resp.arbitration().role().config().UnpackTo(&returned_role_config));
    ASSERT_THAT(role_config, EqualsProto(returned_role_config));
  }
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());

  ASSERT_TRUE(stream2->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  if (!role_name_.empty()) {
    ASSERT_EQ(kRoleName1, resp.arbitration().role().name());
    P4RoleConfig returned_role_config;
    ASSERT_TRUE(
        resp.arbitration().role().config().UnpackTo(&returned_role_config));
    ASSERT_THAT(role_config, EqualsProto(returned_role_config));
  }
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());
  ASSERT_EQ(2, GetNumberOfActiveConnections(kNodeId1));

  //----------------------------------------------------------------------------
  // Controller #2 connects again with the same election_id. Controller #2 will
  // remain master.
  ASSERT_TRUE(stream2->Write(req));

  // Read the mastership info back. Similar to the previous case, it will be
  // sent to Controller #1 and #2.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());

  ASSERT_TRUE(stream2->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());
  ASSERT_EQ(2, GetNumberOfActiveConnections(kNodeId1));

  //----------------------------------------------------------------------------
  // Controller #1 connects again with the same election_id. Controller #2 will
  // remain master.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  if (!role_name_.empty()) {
    req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
    req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
        role_config);
  }
  ASSERT_TRUE(stream1->Write(req));

  // Read the mastership info back. It will be sent to Controller #1 only. It
  // was slave and it is still slave.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());
  ASSERT_EQ(2, GetNumberOfActiveConnections(kNodeId1));

  //----------------------------------------------------------------------------
  // Controller #2 demotes itself and connects with an election_id which is
  // lower than election_id for Controller #1. Note that Controller #1 does not
  // becomes master automatically.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1 - 1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1 - 1));
  if (!role_name_.empty()) {
    req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
    req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
        role_config);
  }
  ASSERT_TRUE(stream2->Write(req));

  // Read the mastership info back. It will be sent to Controller #1 and #2.
  // Status will be non-OK for Controller #1 and #2, as there is no active
  // master. The election ID will be the highest ever seen by the controller so
  // far, i.e. kElectionId2.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::NOT_FOUND, resp.arbitration().status().code());

  ASSERT_TRUE(stream2->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::NOT_FOUND, resp.arbitration().status().code());
  ASSERT_EQ(2, GetNumberOfActiveConnections(kNodeId1));

  //----------------------------------------------------------------------------
  // Controller #2 changes its mind and decides to promote itself again.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId2));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId2));
  if (!role_name_.empty()) {
    req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
    req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
        role_config);
  }
  ASSERT_TRUE(stream2->Write(req));

  // Read the mastership info back. It will be sent to Controller #1 and #2.
  // Status will be OK for Controller #2 and non-OK for Controller #1.

  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());

  ASSERT_TRUE(stream2->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());
  ASSERT_EQ(2, GetNumberOfActiveConnections(kNodeId1));

  //----------------------------------------------------------------------------
  // Controller #2 sends some packet out.
  *req.mutable_packet() = packet2;
  ASSERT_TRUE(stream2->Write(req));

  //----------------------------------------------------------------------------
  // Controller #2 tries to send a malformed packet out.
  *req.mutable_packet() = packet4;
  ASSERT_TRUE(stream2->Write(req));
  ASSERT_TRUE(stream2->Read(&resp));
  ASSERT_EQ(::google::rpc::INVALID_ARGUMENT, resp.error().canonical_code());
  ASSERT_TRUE(ProtoEqual(resp.error().packet_out().packet_out(), packet4));

  //----------------------------------------------------------------------------
  // Controller #2 sends some digest ack.
  *req.mutable_digest_ack() = digest_ack1;
  ASSERT_TRUE(stream2->Write(req));

  //----------------------------------------------------------------------------
  // Controller #1 tries sends some packet out too. However its packet will be
  // dropped as it is not master any more and a stream error will be generated.
  *req.mutable_packet() = packet1;
  ASSERT_TRUE(stream1->Write(req));
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(::google::rpc::PERMISSION_DENIED, resp.error().canonical_code());
  ASSERT_TRUE(ProtoEqual(resp.error().packet_out().packet_out(), packet1));

  //----------------------------------------------------------------------------
  // Controller #1 tries sends some digest ack out too. However its ack will be
  // dropped as it is not master any more and a stream error will be generated.
  *req.mutable_digest_ack() = digest_ack1;
  ASSERT_TRUE(stream1->Write(req));
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(::google::rpc::PERMISSION_DENIED, resp.error().canonical_code());
  ASSERT_TRUE(ProtoEqual(resp.error().digest_list_ack().digest_list_ack(),
                         digest_ack1));

  //----------------------------------------------------------------------------
  // Controller #3 connects. Master will be still Controller #2, as it has the
  // highest election id.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId3));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId3));
  if (!role_name_.empty()) {
    req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
    req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
        role_config);
  }
  ASSERT_TRUE(stream3->Write(req));

  // Read the mastership info back. The data will be sent to Controller #3 only.
  ASSERT_TRUE(stream3->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());
  ASSERT_EQ(3, GetNumberOfActiveConnections(kNodeId1));
  ASSERT_EQ(3, GetNumberOfConnections());

  //----------------------------------------------------------------------------
  // Controller #2 (master) disconnects. This makes the server master-less.
  // We will return the non-ok mastership info back to Controller #1 and
  // Controller #3.
  stream2->WritesDone();
  ASSERT_TRUE(stream2->Finish().ok());

  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::NOT_FOUND, resp.arbitration().status().code());

  ASSERT_TRUE(stream3->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::NOT_FOUND, resp.arbitration().status().code());
  ASSERT_EQ(2, GetNumberOfActiveConnections(kNodeId1));
  ASSERT_EQ(2, GetNumberOfConnections());

  //----------------------------------------------------------------------------
  // Controller #3 promotes itself to master again.
  // Since Controller #3 has a higher election_id, it becomes the new master.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId2));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId2));
  if (!role_name_.empty()) {
    req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
    req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
        role_config);
  }
  ASSERT_TRUE(stream3->Write(req));

  // Read the mastership info back. It will be sent to Controller #1 and #3.
  // Status will be OK for Controller #3 and non-OK for Controller #1.
  ASSERT_TRUE(stream3->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());

  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // We receive some packet from CPU. This will be forwarded to the master
  // which is Controller #3.
  OnPacketReceive(packet3);

  ASSERT_TRUE(stream3->Read(&resp));
  ASSERT_TRUE(ProtoEqual(resp.packet(), packet3));

  //----------------------------------------------------------------------------
  // We receive some packet from CPU. If roles are used, this packet will be
  // filtered out. Otherwise, this will be forwarded to the master which is
  // Controller #3.
  OnPacketReceive(packet5);

  if (role_name_.empty()) {
    ASSERT_TRUE(stream3->Read(&resp));
    ASSERT_TRUE(ProtoEqual(resp.packet(), packet5));
  }

  //----------------------------------------------------------------------------
  // We receive some digest from switch. This will be forwarded to the master
  // which is Controller #3.
  if (role_name_.empty()) {
    OnDigestListReceive(digest_list1);

    ASSERT_TRUE(stream3->Read(&resp));
    ASSERT_TRUE(ProtoEqual(resp.digest(), digest_list1));
  }

  //----------------------------------------------------------------------------
  // Now Controller #1 disconnects. In this case there will be no mastership
  // change. And nothing will be sent to Controller #3 which is still master.
  stream1->WritesDone();
  ASSERT_TRUE(stream1->Finish().ok());
  ASSERT_EQ(1, GetNumberOfActiveConnections(kNodeId1));
  ASSERT_EQ(1, GetNumberOfConnections());

  //----------------------------------------------------------------------------
  // And finally Controller #3 disconnects too. Nothing will be sent.
  stream3->WritesDone();
  ASSERT_TRUE(stream3->Finish().ok());
  ASSERT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  ASSERT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelSuccessForFilteredPacketIn) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  // Role config with disabled PacketIns.
  constexpr char kRoleConfigNoPacketInsText[] = R"pb(
      receives_packet_ins: false
  )pb";
  P4RoleConfig role_config_no_packet_ins;
  CHECK_OK(ParseProtoFromString(kRoleConfigNoPacketInsText,
                                &role_config_no_packet_ins));

  // Sample packet. We dont care about payload.
  ::p4::v1::PacketIn packet;
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata3, packet.add_metadata()));

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  // The Controller connects and becomes master with a role.
  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      role_config_no_packet_ins);
  ASSERT_TRUE(stream->Write(req));

  // Read the mastership info back.
  ASSERT_TRUE(stream->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId1),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId1),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());
  ASSERT_EQ(1, GetNumberOfActiveConnections(kNodeId1));

  // We receive some packet from CPU. This will be dropped as the Controller
  // disabled PacketIns.
  OnPacketReceive(packet);

  // Now the Controller disconnects. We ensure the packet was not sent to it.
  stream->WritesDone();
  ASSERT_FALSE(stream->Read(&resp));
  ASSERT_TRUE(stream->Finish().ok());
  ASSERT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  ASSERT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest,
       StreamChannelSuccessWithRoleConfigCanonicalizesPacketInFilter) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  // Role config with non-canonical filter byte string.
  constexpr char kRoleConfigNotCanonical[] = R"pb(
      receives_packet_ins: true
      packet_in_filter {
        metadata_id: 1
        value: "\x00\xab"  # padded, not canonical.
      }
  )pb";
  P4RoleConfig role_config_not_canonical;
  CHECK_OK(ParseProtoFromString(kRoleConfigNotCanonical,
                                &role_config_not_canonical));

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  // The Controller connects and becomes master with a role.
  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      role_config_not_canonical);
  ASSERT_TRUE(stream->Write(req));

  // Read the mastership info back and check that the filter got canonicalized.
  ASSERT_TRUE(stream->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId1),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId1),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());
  ASSERT_EQ(kRoleName1, resp.arbitration().role().name());
  P4RoleConfig returned_role_config;
  ASSERT_TRUE(
      resp.arbitration().role().config().UnpackTo(&returned_role_config));
  ASSERT_EQ("\xab", returned_role_config.packet_in_filter().value());
  ASSERT_EQ(1, GetNumberOfActiveConnections(kNodeId1));

  // Now the Controller disconnects.
  stream->WritesDone();
  ASSERT_FALSE(stream->Read(&resp));
  ASSERT_TRUE(stream->Finish().ok());
  ASSERT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  ASSERT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureForDuplicateElectionId) {
  ::grpc::ClientContext context1;
  ::grpc::ClientContext context2;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  //----------------------------------------------------------------------------
  // Now start with making the stream channels for all the controllers. We use
  // 2 streams to emulate 2 controllers.
  std::unique_ptr<ClientStreamChannelReaderWriter> stream1 =
      stub_->StreamChannel(&context1);
  std::unique_ptr<ClientStreamChannelReaderWriter> stream2 =
      stub_->StreamChannel(&context2);

  //----------------------------------------------------------------------------
  // Controller #1 connects and becomes master.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  ASSERT_TRUE(stream1->Write(req));

  // Read the mastership info back.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId1),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId1),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // Controller #2 connects and since it has higher election_id it becomes the
  // new master.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId2));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId2));
  ASSERT_TRUE(stream2->Write(req));

  // Read the mastership info back. It will be sent to Controller #1 and #2.
  // Status will be OK for Controller #2 and non-OK for Controller #1.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());

  ASSERT_TRUE(stream2->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // Controller #1 sends same election ID as #2. The request is rejected and
  // Controller #2 will remain master, as it still has the highest election id.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId2));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId2));
  ASSERT_TRUE(stream1->Write(req));

  // Ensure that the request from Controller #1 is rejected and disconnected.
  EXPECT_FALSE(stream1->Read(&resp));
  ::grpc::Status status = stream1->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("is already used by another connection"));
  EXPECT_TRUE(status.error_details().empty());
}

TEST_P(P4ServiceTest, StreamChannelFailureForTooManyConnections) {
  FLAGS_max_num_controller_connections = 2;  // max two connections
  ::grpc::ClientContext context1;
  ::grpc::ClientContext context2;
  ::grpc::ClientContext context3;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillRepeatedly(Return(::util::OkStatus()));

  // The third connection will immediately fail.
  std::unique_ptr<ClientStreamChannelReaderWriter> stream1 =
      stub_->StreamChannel(&context1);
  std::unique_ptr<ClientStreamChannelReaderWriter> stream2 =
      stub_->StreamChannel(&context2);
  std::unique_ptr<ClientStreamChannelReaderWriter> stream3 =
      stub_->StreamChannel(&context3);

  ::grpc::Status status = stream3->Finish();
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Can have max 2"));

  stream1->WritesDone();
  stream2->WritesDone();
  ASSERT_TRUE(stream1->Finish().ok());
  ASSERT_TRUE(stream2->Finish().ok());
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureForZeroDeviceId) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);

  ASSERT_TRUE(stream->Write(req));
  ASSERT_FALSE(stream->Read(&resp));  // no resp is sent back
  stream->WritesDone();
  ::grpc::Status status = stream->Finish();
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureForInvalidDeviceId) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);

  req.mutable_arbitration()->set_device_id(kNodeId1 + 1);  // unknown node id
  ASSERT_TRUE(stream->Write(req));
  ASSERT_FALSE(stream->Read(&resp));  // no resp is sent back
  stream->WritesDone();
  ::grpc::Status status = stream->Finish();
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureForZeroElectionId) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);

  req.mutable_arbitration()->set_device_id(kNodeId1);
  ASSERT_TRUE(stream->Write(req));
  ASSERT_FALSE(stream->Read(&resp));  // no resp is sent back
  stream->WritesDone();
  ::grpc::Status status = stream->Finish();
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureWhenRegisterHandlerFails) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kOperErrorMsg)));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);

  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  ASSERT_TRUE(stream->Write(req));
  ASSERT_FALSE(stream->Read(&resp));  // no resp is sent back
  stream->WritesDone();
  ::grpc::Status status = stream->Finish();
  EXPECT_EQ(::grpc::StatusCode::INTERNAL, status.error_code());
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureForTooManyControllersPerNode) {
  FLAGS_max_num_controllers_per_node = 1;  // max one controller per node.
  ::grpc::ClientContext context1;
  ::grpc::ClientContext context2;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  req.mutable_arbitration()->set_device_id(kNodeId1);

  // Connect the 1st controller.
  std::unique_ptr<ClientStreamChannelReaderWriter> stream1 =
      stub_->StreamChannel(&context1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  ASSERT_TRUE(stream1->Write(req));
  ASSERT_TRUE(stream1->Read(&resp));

  // Now try to connect the 2nd one. This will fail and the connection will be
  // closed.
  std::unique_ptr<ClientStreamChannelReaderWriter> stream2 =
      stub_->StreamChannel(&context2);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId2));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId2));
  ASSERT_TRUE(stream2->Write(req));
  ASSERT_FALSE(stream2->Read(&resp));  // no resp back
  stream2->WritesDone();
  ::grpc::Status status = stream2->Finish();
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, status.error_code());

  // Disconnect the 1st controller at the end.
  stream1->WritesDone();
  ASSERT_TRUE(stream1->Finish().ok());
}

TEST_P(P4ServiceTest, StreamChannelFailureForInvalidRoleConfigType) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);

  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
  req.mutable_arbitration()->mutable_role()->mutable_config()->set_type_url(
      "some_type_url");
  ASSERT_TRUE(stream->Write(req));
  ASSERT_FALSE(stream->Read(&resp));  // no resp is sent back
  stream->WritesDone();
  ::grpc::Status status = stream->Finish();
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureForEmptyRoleConfigPacketFilter) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  // Role config with an empty filter byte string.
  constexpr char kRoleConfigEmptyFilter[] = R"pb(
      receives_packet_ins: true
      packet_in_filter {
        metadata_id: 1
        value: ""  # empty
      }
  )pb";
  P4RoleConfig role_config_empty_filter;
  CHECK_OK(
      ParseProtoFromString(kRoleConfigEmptyFilter, &role_config_empty_filter));

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  // The Controller connects and becomes master with a role.
  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      role_config_empty_filter);
  ASSERT_TRUE(stream->Write(req));

  ASSERT_FALSE(stream->Read(&resp));
  stream->WritesDone();
  ::grpc::Status status = stream->Finish();
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("contains an empty PacketIn filter"));
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureForRoleChange) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);

  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      GetRoleConfig());
  ASSERT_TRUE(stream->Write(req));
  ASSERT_TRUE(stream->Read(&resp));

  // Try to change the controllers role by name.
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName2);
  req.mutable_arbitration()->mutable_role()->clear_config();
  ASSERT_TRUE(stream->Write(req));
  ASSERT_FALSE(stream->Read(&resp));
  stream->WritesDone();
  ::grpc::Status status = stream->Finish();
  EXPECT_EQ(::grpc::StatusCode::FAILED_PRECONDITION, status.error_code());
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureForRoleConfigOnDefaultRole) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);

  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      GetRoleConfig());
  ASSERT_TRUE(stream->Write(req));
  ASSERT_FALSE(stream->Read(&resp));
  stream->WritesDone();
  ::grpc::Status status = stream->Finish();
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("default role"));
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

TEST_P(P4ServiceTest, StreamChannelFailureForOverlappingExclusiveRoles) {
  ::grpc::ClientContext context1;
  ::grpc::ClientContext context2;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  // Role configs with overlapping exclusive IDs.
  constexpr char kRoleConfigText1[] = R"pb(
      exclusive_p4_ids: 30
      exclusive_p4_ids: 12
  )pb";
  constexpr char kRoleConfigText2[] = R"pb(
      exclusive_p4_ids: 44
      exclusive_p4_ids: 30
  )pb";

  P4RoleConfig role_config1;
  CHECK_OK(ParseProtoFromString(kRoleConfigText1, &role_config1));
  P4RoleConfig role_config2;
  CHECK_OK(ParseProtoFromString(kRoleConfigText2, &role_config2));

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream1 =
      stub_->StreamChannel(&context1);
  std::unique_ptr<ClientStreamChannelReaderWriter> stream2 =
      stub_->StreamChannel(&context2);

  //----------------------------------------------------------------------------
  // Controller #1 connects and becomes master for role 1.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      role_config1);
  ASSERT_TRUE(stream1->Write(req));

  // Read the mastership info back.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // Controller #2 connects and sends a role config that has overlapping
  // exclusive IDs with controller #1.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName2);
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      role_config2);
  ASSERT_TRUE(stream2->Write(req));

  // The stream of controller #2 gets closed and the status will be non-OK.
  ASSERT_FALSE(stream2->Read(&resp));
  stream2->WritesDone();
  ::grpc::Status status = stream2->Finish();
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr(
          "contains exclusive IDs that overlap with existing exclusive IDs"));
}

TEST_P(P4ServiceTest, StreamChannelFailureForOverlappingSharedRoles) {
  ::grpc::ClientContext context1;
  ::grpc::ClientContext context2;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  // Role configs with overlapping shared IDs.
  constexpr char kRoleConfigText1[] = R"pb(
      exclusive_p4_ids: 12
      shared_p4_ids: 79
  )pb";
  constexpr char kRoleConfigText2[] = R"pb(
      exclusive_p4_ids: 45
      shared_p4_ids: 12
  )pb";

  P4RoleConfig role_config1;
  CHECK_OK(ParseProtoFromString(kRoleConfigText1, &role_config1));
  P4RoleConfig role_config2;
  CHECK_OK(ParseProtoFromString(kRoleConfigText2, &role_config2));

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream1 =
      stub_->StreamChannel(&context1);
  std::unique_ptr<ClientStreamChannelReaderWriter> stream2 =
      stub_->StreamChannel(&context2);

  //----------------------------------------------------------------------------
  // Controller #1 connects and becomes master for role 1.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      role_config1);
  ASSERT_TRUE(stream1->Write(req));

  // Read the mastership info back.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // Controller #2 connects and sends a role config that has overlapping shared
  // IDs with controller #1.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName2);
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      role_config2);
  ASSERT_TRUE(stream2->Write(req));

  // The stream of controller #2 gets closed and the status will be non-OK.
  ASSERT_FALSE(stream2->Read(&resp));
  stream2->WritesDone();
  ::grpc::Status status = stream2->Finish();
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr(
          "contains shared IDs that overlap with existing exclusive IDs"));
}

TEST_P(P4ServiceTest, StreamChannelFailureForInvalidRoleConfigPacketInFlag) {
  // Role config with filter but disabled PacketIns.
  constexpr char kRoleConfigText[] = R"pb(
    packet_in_filter {
      metadata_id: 666666
      value: "\x12"
    }
    receives_packet_ins: false
  )pb";
  P4RoleConfig role_config;
  CHECK_OK(ParseProtoFromString(kRoleConfigText, &role_config));

  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterStreamMessageResponseWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));

  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub_->StreamChannel(&context);

  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  req.mutable_arbitration()->mutable_role()->set_name(kRoleName1);
  req.mutable_arbitration()->mutable_role()->mutable_config()->PackFrom(
      role_config);
  ASSERT_TRUE(stream->Write(req));
  ASSERT_FALSE(stream->Read(&resp));
  stream->WritesDone();
  ::grpc::Status status = stream->Finish();
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr("contains a PacketIn filter, but disables PacketIn delivery"));
  EXPECT_EQ(0, GetNumberOfActiveConnections(kNodeId1));
  EXPECT_EQ(0, GetNumberOfConnections());
}

// Pushing a different forwarding pipeline config again should work.
TEST_P(P4ServiceTest, PushForwardingPipelineConfigWithCookieSuccess) {
  ForwardingPipelineConfigs configs;
  FillTestForwardingPipelineConfigsAndSave(&configs);

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "SetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "GetForwardingPipelineConfig", _))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_CALL(*switch_mock_, PushForwardingPipelineConfig(_, _))
      .WillRepeatedly(Return(::util::OkStatus()));

  ::grpc::ServerContext context;
  StreamMessageReaderWriterMock stream;
  p4runtime::SdnConnection controller(&context, &stream);
  controller.SetElectionId(kElectionId1);
  AddFakeMasterController(kNodeId1, &controller);
  ::p4::v1::SetForwardingPipelineConfigRequest setRequest;
  ::p4::v1::SetForwardingPipelineConfigResponse setResponse;
  setRequest.set_device_id(kNodeId1);
  setRequest.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  setRequest.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  setRequest.set_role(role_name_);
  setRequest.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  *setRequest.mutable_config() = configs.node_id_to_config().at(kNodeId1);

  setRequest.mutable_config()->mutable_cookie()->set_cookie(kCookie1);

  // Setting pipeline config
  ::grpc::Status status = p4_service_->SetForwardingPipelineConfig(
      &context, &setRequest, &setResponse);
  EXPECT_TRUE(status.ok()) << "Error: " << status.error_message();

  // Retrieving the pipeline config
  ::p4::v1::GetForwardingPipelineConfigRequest getRequest;
  ::p4::v1::GetForwardingPipelineConfigResponse getResponse;
  getRequest.set_device_id(kNodeId1);
  getRequest.set_response_type(
      ::p4::v1::GetForwardingPipelineConfigRequest::COOKIE_ONLY);
  status = p4_service_->GetForwardingPipelineConfig(&context, &getRequest,
                                                    &getResponse);

  EXPECT_TRUE(status.ok()) << "Error: " << status.error_message();

  // Validating cookie value
  EXPECT_TRUE(getResponse.config().cookie().cookie() == kCookie1)
      << "Error: Cookie 1 " << getResponse.config().cookie().cookie()
      << " not equal " << kCookie1;

  ASSERT_OK(p4_service_->Teardown());
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

TEST_P(P4ServiceTest, GetCapabilities) {
  ::grpc::ServerContext context;
  ::p4::v1::CapabilitiesRequest request;
  ::p4::v1::CapabilitiesResponse response;
  ::grpc::Status status =
      p4_service_->Capabilities(&context, &request, &response);
  EXPECT_TRUE(status.ok()) << "Error: " << status.error_message();
  ASSERT_EQ(response.p4runtime_api_version(), STRINGIFY(P4RUNTIME_VER));
}

INSTANTIATE_TEST_SUITE_P(
    P4ServiceTestWithMode, P4ServiceTest,
    ::testing::Combine(::testing::Values(OPERATION_MODE_STANDALONE,
                                         OPERATION_MODE_COUPLED,
                                         OPERATION_MODE_SIM),
                       ::testing::Values(true, false) /* with role config */));

}  // namespace hal
}  // namespace stratum
