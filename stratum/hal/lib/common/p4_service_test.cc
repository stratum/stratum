// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/common/p4_service.h"

#include <memory>

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
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

DECLARE_int32(max_num_controllers_per_node);
DECLARE_int32(max_num_controller_connections);
DECLARE_string(forwarding_pipeline_configs_file);
DECLARE_string(write_req_log_file);
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

class P4ServiceTest : public ::testing::TestWithParam<OperationMode> {
 protected:
  void SetUp() override {
    mode_ = GetParam();
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
    // Before starting the tests, remove the write req file if exists.
    if (PathExists(FLAGS_write_req_log_file)) {
      ASSERT_OK(RemoveFile(FLAGS_write_req_log_file));
    }
  }

  void TearDown() override { server_->Shutdown(); }

  void OnPacketReceive(const ::p4::v1::PacketIn& packet) {
    p4_service_->PacketReceiveHandler(kNodeId1, packet);
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

  void AddFakeMasterController(uint64 node_id, uint64 connection_id,
                               absl::uint128 election_id,
                               const std::string& uri) {
    absl::WriterMutexLock l(&p4_service_->controller_lock_);
    P4Service::Controller controller(connection_id, election_id, uri, nullptr);
    p4_service_->node_id_to_controllers_[node_id].insert(controller);
  }

  static constexpr char kForwardingPipelineConfigsTemplate[] = R"(
      node_id_to_config {
        key: $0
        value {
          p4info {
            tables {
              preamble {
                name: "some_table"
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
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  configs.mutable_node_id_to_config()->at(kNodeId1).set_p4_device_config(
      "fake");  // emulate a modification in the config
  *request.mutable_config() = configs.node_id_to_config().at(kNodeId1);
  AddFakeMasterController(kNodeId1, 1, kElectionId1, "some uri");

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
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_action(::p4::v1::SetForwardingPipelineConfigRequest::VERIFY);
  *request.mutable_config() = configs.node_id_to_config().at(kNodeId1);
  AddFakeMasterController(kNodeId1, 1, kElectionId1, "some uri");

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
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  AddFakeMasterController(kNodeId1, 1, kElectionId1, "some uri");

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
  ::p4::v1::SetForwardingPipelineConfigRequest request;
  ::p4::v1::SetForwardingPipelineConfigResponse response;
  request.set_device_id(kNodeId1);
  request.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  request.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  request.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  AddFakeMasterController(kNodeId1, 1, kElectionId1, "some uri");

  ::grpc::Status status =
      p4_service_->SetForwardingPipelineConfig(&context, &request, &response);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("reboot required"));

  // CheckForwardingPipelineConfigs(&configs, kNodeId1);
  ASSERT_OK(p4_service_->Teardown());
  CheckForwardingPipelineConfigs(nullptr, 0 /*ignored*/);
}

TEST_P(P4ServiceTest, WriteSuccess) {
  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.set_device_id(kNodeId1);
  req.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  req.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  req.add_updates()->set_type(::p4::v1::Update::INSERT);
  AddFakeMasterController(kNodeId1, 1, kElectionId1, "some uri");

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
  ::grpc::ClientContext context;
  ::p4::v1::WriteRequest req;
  ::p4::v1::WriteResponse resp;
  req.set_device_id(kNodeId1);
  req.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  req.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  req.add_updates()->set_type(::p4::v1::Update::INSERT);
  req.add_updates()->set_type(::p4::v1::Update::MODIFY);
  AddFakeMasterController(kNodeId1, 1, kElectionId1, "some uri");

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

TEST_P(P4ServiceTest, ReadSuccess) {
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
  req.add_entities()->mutable_table_entry()->set_table_id(kTableId1);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, ReadForwardingEntries(EqualsProto(req), _, _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_TRUE(status.ok());
}

TEST_P(P4ServiceTest, ReadSuccessForNoEntitiesToRead) {
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("P4Service", "Read", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
      stub_->Read(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_TRUE(status.ok());
}

TEST_P(P4ServiceTest, ReadFailureForNoDeviceId) {
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
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
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
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
}

TEST_P(P4ServiceTest, ReadFailureForAuthError) {
  ::grpc::ClientContext context;
  ::p4::v1::ReadRequest req;
  ::p4::v1::ReadResponse resp;
  req.set_device_id(kNodeId1);
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

  // Sample packets. We dont care about payload.
  ::p4::v1::PacketOut packet1;
  ::p4::v1::PacketOut packet2;
  ::p4::v1::PacketIn packet3;
  ::p4::v1::PacketOut packet4;
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata1, packet1.add_metadata()));
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata2, packet2.add_metadata()));
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata3, packet3.add_metadata()));
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata4, packet4.add_metadata()));

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterPacketReceiveWriter(kNodeId1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, TransmitPacket(kNodeId1, EqualsProto(packet2)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, TransmitPacket(kNodeId1, EqualsProto(packet4)))
      .WillOnce(Return(::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                                      kOperErrorMsg)));

  //----------------------------------------------------------------------------
  // Before any connection, any packet received from the controller will be
  // ignored.
  OnPacketReceive(packet3);

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
  ASSERT_TRUE(stream1->Write(req));

  // Read the mastership info back.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId1),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId1),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // Controller #2 connect and since it has higher election_id it becomes the
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

  //----------------------------------------------------------------------------
  // Controller #1 connects again with the same election_id. Controller #2 will
  // remain master.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1));
  ASSERT_TRUE(stream1->Write(req));

  // Read the mastership info back. It will be sent to Controller #1 only. It
  // was slave and it is still slave.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // Controller #2 demotes itself and connects with an election_id which is
  // lower than election_id for Controller #1. In this case Controller #1
  // becomes master.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId1 - 1));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId1 - 1));
  ASSERT_TRUE(stream2->Write(req));

  // Read the mastership info back. It will be sent to Controller #1 and #2.
  // Status will be OK for Controller #1 and non-OK for Controller #2.
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId1),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId1),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());

  ASSERT_TRUE(stream2->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId1),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId1),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // Controller #2 changes its mind and decides to promote itself again.
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
  // Controller #1 tries sends some packet out too. However its packet will be
  // dropped as it is not master any more and a stream error will be generated.
  *req.mutable_packet() = packet1;
  ASSERT_TRUE(stream1->Write(req));
  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(::google::rpc::PERMISSION_DENIED, resp.error().canonical_code());
  ASSERT_TRUE(ProtoEqual(resp.error().packet_out().packet_out(), packet1));

  //----------------------------------------------------------------------------
  // Controller #3 connects. Master will be still Controller #2, as it has the
  // highest election id.
  req.mutable_arbitration()->set_device_id(kNodeId1);
  req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(kElectionId3));
  req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(kElectionId3));
  ASSERT_TRUE(stream3->Write(req));

  // Read the mastership info back. The data will be sent to Controller #3 only.
  ASSERT_TRUE(stream3->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId2),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId2),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // Controller #2 (master) disconnects. This triggers a mastership change.
  // We will return the mastership info back to Controller #1 and Controller #3.
  // Since Controller #3 has a higher election_id, it becomes the new master.
  stream2->WritesDone();
  ASSERT_TRUE(stream2->Finish().ok());

  ASSERT_TRUE(stream1->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId3),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId3),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::ALREADY_EXISTS, resp.arbitration().status().code());

  ASSERT_TRUE(stream3->Read(&resp));
  ASSERT_EQ(absl::Uint128High64(kElectionId3),
            resp.arbitration().election_id().high());
  ASSERT_EQ(absl::Uint128Low64(kElectionId3),
            resp.arbitration().election_id().low());
  ASSERT_EQ(::google::rpc::OK, resp.arbitration().status().code());

  //----------------------------------------------------------------------------
  // We receive some packet from CPU. This will be forwarded to the master
  // which is Controller #3.
  OnPacketReceive(packet3);

  ASSERT_TRUE(stream3->Read(&resp));
  ASSERT_TRUE(ProtoEqual(resp.packet(), packet3));

  //----------------------------------------------------------------------------
  // Now Controller #1 disconnects. In this case there will be no mastership
  // change. And nothing will be sent to Controller #3 which is still master.
  stream1->WritesDone();
  ASSERT_TRUE(stream1->Finish().ok());

  //----------------------------------------------------------------------------
  // And finally Controller #3 disconnects too. Nothing will be sent.
  stream3->WritesDone();
  ASSERT_TRUE(stream3->Finish().ok());
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
}

TEST_P(P4ServiceTest, StreamChannelFailureWhenRegisterHandlerFails) {
  ::grpc::ClientContext context;
  ::p4::v1::StreamMessageRequest req;
  ::p4::v1::StreamMessageResponse resp;

  EXPECT_CALL(*auth_policy_checker_mock_,
              Authorize("P4Service", "StreamChannel", _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterPacketReceiveWriter(kNodeId1, _))
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
  EXPECT_CALL(*switch_mock_, RegisterPacketReceiveWriter(kNodeId1, _))
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
  ::p4::v1::SetForwardingPipelineConfigRequest setRequest;
  ::p4::v1::SetForwardingPipelineConfigResponse setResponse;
  setRequest.set_device_id(kNodeId1);
  setRequest.mutable_election_id()->set_high(absl::Uint128High64(kElectionId1));
  setRequest.mutable_election_id()->set_low(absl::Uint128Low64(kElectionId1));
  setRequest.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  *setRequest.mutable_config() = configs.node_id_to_config().at(kNodeId1);
  AddFakeMasterController(kNodeId1, 1, kElectionId1, "some uri");

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

INSTANTIATE_TEST_SUITE_P(P4ServiceTestWithMode, P4ServiceTest,
                         ::testing::Values(OPERATION_MODE_STANDALONE,
                                           OPERATION_MODE_COUPLED,
                                           OPERATION_MODE_SIM));

}  // namespace hal
}  // namespace stratum
