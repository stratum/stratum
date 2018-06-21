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


#include "stratum/hal/lib/common/hal.h"

#include "gflags/gflags.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/switch_mock.h"
//#include "stratum/lib/sandcastle/procmon_service.grpc.pb.h"
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/security/credentials_manager_mock.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/substitute.h"

DECLARE_bool(warmboot);
DECLARE_string(chassis_config_file);
DECLARE_string(forwarding_pipeline_configs_file);
DECLARE_string(test_tmpdir);
DECLARE_string(url);
DECLARE_string(local_hercules_url);
DECLARE_string(procmon_service_addr);
DECLARE_string(persistent_config_dir);

namespace stratum {
namespace hal {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

// A fake implementation of ProcmonService for unit testing.
class FakeProcmonService final : public procmon::ProcmonService::Service {
 public:
  FakeProcmonService() : pid_(-1) {}
  ~FakeProcmonService() override {}

  void SetPid(int pid) { pid_ = pid; }

  ::grpc::Status Checkin(::grpc::ServerContext* context,
                         const procmon::CheckinRequest* req,
                         procmon::CheckinResponse* resp) override {
    // Fake a behavior where for checkin_key = kGoodPid we return OK and for
    // other checkin_keys we return error.
    if (req->checkin_key() == pid_) {
      return ::grpc::Status::OK;
    }
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Invalid pid.");
  }

  // FakeProcmonService is neither copyable nor movable.
  FakeProcmonService(const FakeProcmonService&) = delete;
  FakeProcmonService& operator=(const FakeProcmonService&) = delete;

 private:
  int pid_;
};

class HalTest : public ::testing::Test {
 protected:
  // Per-test-case set-up.
  static void SetUpTestCase() {
    // Setup Hal class instance under test.
    FLAGS_url = "localhost:" + std::to_string(PickUnusedIpv4PortOrDie());
    FLAGS_local_hercules_url =
        "localhost:" + std::to_string(PickUnusedIpv4PortOrDie());
    FLAGS_chassis_config_file = FLAGS_test_tmpdir + "/chassis_config.pb.txt";
    FLAGS_forwarding_pipeline_configs_file =
        FLAGS_test_tmpdir + "/forwarding_pipeline_configs_file.pb.txt";
    FLAGS_persistent_config_dir = FLAGS_test_tmpdir + "/config_dir";
    switch_mock_ = new ::testing::StrictMock<SwitchMock>();
    auth_policy_checker_mock_ =
        new ::testing::StrictMock<AuthPolicyCheckerMock>();
    credentials_manager_mock_ =
        new ::testing::StrictMock<CredentialsManagerMock>();
    hal_ = Hal::CreateSingleton(kMode, switch_mock_, auth_policy_checker_mock_,
                                credentials_manager_mock_);
    ASSERT_TRUE(hal_ != nullptr);

    // Create and start a FakeProcmonService instance for testing purpose.
    FLAGS_procmon_service_addr =
        "localhost:" + std::to_string(PickUnusedIpv4PortOrDie());
    procmon_service_ = new FakeProcmonService();
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(FLAGS_procmon_service_addr,
                             ::grpc::InsecureServerCredentials());
    builder.RegisterService(procmon_service_);
    procmon_server_ = builder.BuildAndStart().release();
  }

  // Per-test-case tear-down.
  static void TearDownTestCase() {
    procmon_server_->Shutdown(std::chrono::system_clock::now());
    delete switch_mock_;
    delete auth_policy_checker_mock_;
    delete credentials_manager_mock_;
    delete procmon_service_;
    delete procmon_server_;
    switch_mock_ = nullptr;
    auth_policy_checker_mock_ = nullptr;
    credentials_manager_mock_ = nullptr;
    procmon_service_ = nullptr;
    procmon_server_ = nullptr;
  }

  void SetUp() override { hal_->ClearErrors(); }

  void FillTestChassisConfigAndSave(ChassisConfig* chassis_config) {
    const std::string& chassis_config_text = absl::Substitute(
        kChassisConfigTemplate, kNodeId1, kUnit1 + 1, kNodeId2, kUnit2 + 1);
    ASSERT_OK(ParseProtoFromString(chassis_config_text, chassis_config));
    ASSERT_OK(
        WriteStringToFile(chassis_config_text, FLAGS_chassis_config_file));
  }

  void FillTestForwardingPipelineConfigsAndSave(
      ForwardingPipelineConfigs* forwarding_pipeline_configs) {
    const std::string& forwarding_pipeline_configs_text = absl::Substitute(
        kForwardingPipelineConfigsTemplate, kNodeId1, kNodeId2);
    ASSERT_OK(ParseProtoFromString(forwarding_pipeline_configs_text,
                                   forwarding_pipeline_configs));
    ASSERT_OK(WriteStringToFile(forwarding_pipeline_configs_text,
                                FLAGS_forwarding_pipeline_configs_file));
  }

  static constexpr char kChassisConfigTemplate[] = R"(
      description: "Sample test config."
      nodes {
        id:  $0
        slot: 1
        index: $1
      }
      nodes {
        id:  $2
        slot: 1
        index: $3
      }
  )";

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
  static constexpr char kErrorMsg[] = "Some error";
  static constexpr uint64 kNodeId1 = 123123123;
  static constexpr uint64 kNodeId2 = 456456456;
  static constexpr int kUnit1 = 0;
  static constexpr int kUnit2 = 1;
  static constexpr OperationMode kMode = OPERATION_MODE_STANDALONE;
  static ::testing::StrictMock<SwitchMock>* switch_mock_;
  static ::testing::StrictMock<AuthPolicyCheckerMock>*
      auth_policy_checker_mock_;
  static ::testing::StrictMock<CredentialsManagerMock>*
      credentials_manager_mock_;
  static Hal* hal_;  // pointer which points to the singleton instance
  static FakeProcmonService* procmon_service_;
  static ::grpc::Server* procmon_server_;
};

constexpr char HalTest::kChassisConfigTemplate[];
constexpr char HalTest::kForwardingPipelineConfigsTemplate[];
constexpr char HalTest::kErrorMsg[];
constexpr uint64 HalTest::kNodeId1;
constexpr uint64 HalTest::kNodeId2;
constexpr int HalTest::kUnit1;
constexpr int HalTest::kUnit2;
constexpr OperationMode HalTest::kMode;
::testing::StrictMock<SwitchMock>* HalTest::switch_mock_ = nullptr;
::testing::StrictMock<AuthPolicyCheckerMock>*
    HalTest::auth_policy_checker_mock_ = nullptr;
::testing::StrictMock<CredentialsManagerMock>*
    HalTest::credentials_manager_mock_ = nullptr;
Hal* HalTest::hal_ = nullptr;
FakeProcmonService* HalTest::procmon_service_ = nullptr;
::grpc::Server* HalTest::procmon_server_ = nullptr;

TEST_F(HalTest, ColdbootSetupSuccessForSavedConfigs) {
  // Setup and save the test config(s).
  ChassisConfig chassis_config;
  ForwardingPipelineConfigs forwarding_pipeline_configs;
  FillTestChassisConfigAndSave(&chassis_config);
  FillTestForwardingPipelineConfigsAndSave(&forwarding_pipeline_configs);

  EXPECT_CALL(*switch_mock_, PushChassisConfig(EqualsProto(chassis_config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId1,
          EqualsProto(
              forwarding_pipeline_configs.node_id_to_config().at(kNodeId1))))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId2,
          EqualsProto(
              forwarding_pipeline_configs.node_id_to_config().at(kNodeId2))))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  FLAGS_warmboot = false;
  ASSERT_OK(hal_->Setup());
  const auto& errors = hal_->GetErrors();
  EXPECT_TRUE(errors.empty());
}

TEST_F(HalTest, ColdbootSetupSuccessForNoSavedConfigAtAll) {
  // Delete all the saved chassis config. There will be no config push at all.
  if (PathExists(FLAGS_chassis_config_file)) {
    ASSERT_OK(RemoveFile(FLAGS_chassis_config_file));
  }
  if (PathExists(FLAGS_forwarding_pipeline_configs_file)) {
    ASSERT_OK(RemoveFile(FLAGS_forwarding_pipeline_configs_file));
  }

  // Call and validate results.
  FLAGS_warmboot = false;
  ASSERT_OK(hal_->Setup());
  const auto& errors = hal_->GetErrors();
  EXPECT_TRUE(errors.empty());
}

TEST_F(HalTest, ColdbootSetupSuccessForNoForwardingPipelineConfig) {
  // Save the chassis config but delete the saved forwarding pipeline config.
  // There will be chassis config push but no forwarding pipeline config push.
  ChassisConfig chassis_config;
  FillTestChassisConfigAndSave(&chassis_config);
  if (PathExists(FLAGS_forwarding_pipeline_configs_file)) {
    ASSERT_OK(RemoveFile(FLAGS_forwarding_pipeline_configs_file));
  }

  EXPECT_CALL(*switch_mock_, PushChassisConfig(EqualsProto(chassis_config)))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  FLAGS_warmboot = false;
  ASSERT_OK(hal_->Setup());
  const auto& errors = hal_->GetErrors();
  EXPECT_TRUE(errors.empty());
}

TEST_F(HalTest, ColdbootSetupSuccessForNoChassisConfig) {
  // Save the forwarding pipeline config but delete the saved chassis config.
  // There will be forwarding pipeline config push but no chassis config push.
  ForwardingPipelineConfigs forwarding_pipeline_configs;
  FillTestForwardingPipelineConfigsAndSave(&forwarding_pipeline_configs);
  if (PathExists(FLAGS_chassis_config_file)) {
    ASSERT_OK(RemoveFile(FLAGS_chassis_config_file));
  }

  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId1,
          EqualsProto(
              forwarding_pipeline_configs.node_id_to_config().at(kNodeId1))))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId2,
          EqualsProto(
              forwarding_pipeline_configs.node_id_to_config().at(kNodeId2))))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  FLAGS_warmboot = false;
  ASSERT_OK(hal_->Setup());
  const auto& errors = hal_->GetErrors();
  EXPECT_TRUE(errors.empty());
}

TEST_F(HalTest, ColdbootSetupFailureWhenChassisConfigPushFails) {
  // Setup and save the test config(s).
  ChassisConfig chassis_config;
  ForwardingPipelineConfigs forwarding_pipeline_configs;
  FillTestChassisConfigAndSave(&chassis_config);
  FillTestForwardingPipelineConfigsAndSave(&forwarding_pipeline_configs);

  EXPECT_CALL(*switch_mock_, PushChassisConfig(EqualsProto(chassis_config)))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));

  // Call and validate results.
  FLAGS_warmboot = false;
  ::util::Status status = hal_->Setup();
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
  const auto& errors = hal_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg));
  EXPECT_THAT(errors[0].error_message(), HasSubstr("saved chassis config"));
}

TEST_F(HalTest, ColdbootSetupFailureWhenPipelineConfigPushFailsForSomeNodes) {
  // Setup and save the test config(s).
  ChassisConfig chassis_config;
  ForwardingPipelineConfigs forwarding_pipeline_configs;
  FillTestChassisConfigAndSave(&chassis_config);
  FillTestForwardingPipelineConfigsAndSave(&forwarding_pipeline_configs);

  EXPECT_CALL(*switch_mock_, PushChassisConfig(EqualsProto(chassis_config)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId1,
          EqualsProto(
              forwarding_pipeline_configs.node_id_to_config().at(kNodeId1))))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));
  EXPECT_CALL(
      *switch_mock_,
      PushForwardingPipelineConfig(
          kNodeId2,
          EqualsProto(
              forwarding_pipeline_configs.node_id_to_config().at(kNodeId2))))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  FLAGS_warmboot = false;
  ::util::Status status = hal_->Setup();
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
  const auto& errors = hal_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg));
  EXPECT_THAT(errors[0].error_message(),
              HasSubstr("saved forwarding pipeline configs"));
}

TEST_F(HalTest, WarmbootSetupSuccessForSavedConfig) {
  // Setup and save the test config(s).
  ChassisConfig chassis_config;
  ForwardingPipelineConfigs forwarding_pipeline_configs;
  FillTestChassisConfigAndSave(&chassis_config);
  FillTestForwardingPipelineConfigsAndSave(&forwarding_pipeline_configs);

  EXPECT_CALL(*switch_mock_, Unfreeze()).WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  FLAGS_warmboot = true;
  ASSERT_OK(hal_->Setup());
  const auto& errors = hal_->GetErrors();
  EXPECT_TRUE(errors.empty());
}

TEST_F(HalTest, WarmbootbootSetupFailureForNoSavedConfig) {
  // Delete the saved chassis config. There will be no chassis config push and
  // the call will fail.
  if (PathExists(FLAGS_chassis_config_file)) {
    ASSERT_OK(RemoveFile(FLAGS_chassis_config_file));
  }

  // Call and validate results.
  FLAGS_warmboot = true;
  ::util::Status status = hal_->Setup();
  ASSERT_EQ(ERR_FILE_NOT_FOUND, status.error_code());
  const auto& errors = hal_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr("saved chassis config"));
}

TEST_F(HalTest, WarmbootSetupFailureWhenUnfreezeFails) {
  // Setup and save the test config(s).
  ChassisConfig chassis_config;
  ForwardingPipelineConfigs forwarding_pipeline_configs;
  FillTestChassisConfigAndSave(&chassis_config);
  FillTestForwardingPipelineConfigsAndSave(&forwarding_pipeline_configs);

  EXPECT_CALL(*switch_mock_, Unfreeze())
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));

  // Call and validate results.
  FLAGS_warmboot = true;
  ::util::Status status = hal_->Setup();
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
  const auto& errors = hal_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg));
  EXPECT_THAT(errors[0].error_message(), HasSubstr("unfreeze"));
}

TEST_F(HalTest, ColdbootTeardownSuccess) {
  EXPECT_CALL(*switch_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*auth_policy_checker_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, UnregisterEventNotifyWriter())
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  FLAGS_warmboot = false;
  ASSERT_OK(hal_->Teardown());
  const auto& errors = hal_->GetErrors();
  EXPECT_TRUE(errors.empty());
}

TEST_F(HalTest, ColdbootTeardownFailureWhenSwitchInterfaceShutdownFails) {
  EXPECT_CALL(*switch_mock_, Shutdown())
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));
  EXPECT_CALL(*auth_policy_checker_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  FLAGS_warmboot = false;
  ::util::Status status = hal_->Teardown();
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
  const auto& errors = hal_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg));
  EXPECT_THAT(errors[0].error_message(), HasSubstr("shutdown"));
}

TEST_F(HalTest, ColdbootTeardownFailureWhenAuthPolicyCheckerShutdownFails) {
  EXPECT_CALL(*switch_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*auth_policy_checker_mock_, Shutdown())
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));

  // Call and validate results.
  FLAGS_warmboot = false;
  ::util::Status status = hal_->Teardown();
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
  const auto& errors = hal_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg));
  EXPECT_THAT(errors[0].error_message(), HasSubstr("shutdown"));
}

TEST_F(HalTest, WarmbootTeardownSuccess) {
  EXPECT_CALL(*switch_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*auth_policy_checker_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results. The warmboot flag is not used in this case. A
  // call to Teardown will always call Shutdown() in switch_interface.
  FLAGS_warmboot = true;
  ASSERT_OK(hal_->Teardown());
  const auto& errors = hal_->GetErrors();
  EXPECT_TRUE(errors.empty());
}

TEST_F(HalTest, WarmbootTeardownFailure) {
  EXPECT_CALL(*switch_mock_, Shutdown())
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));
  EXPECT_CALL(*auth_policy_checker_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results. The warmboot flag is not used in this case. A
  // call to Teardown will always call Shutdown() in switch_interface.
  FLAGS_warmboot = true;
  ::util::Status status = hal_->Teardown();
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
  const auto& errors = hal_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg));
  EXPECT_THAT(errors[0].error_message(), HasSubstr("shutdown"));
}

namespace {

void* TestShutdownThread(void* arg) {
  sleep(3);  // some sleep to emulate a task.
  static_cast<Hal*>(arg)->HandleSignal(SIGINT);
  return nullptr;
}

}  // namespace

TEST_F(HalTest, StartAndShutdownServerWhenProcmonCheckinSucceeds) {
  EXPECT_CALL(*switch_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*auth_policy_checker_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*credentials_manager_mock_,
              GenerateExternalFacingServerCredentials())
      .WillOnce(Return(::grpc::InsecureServerCredentials()));
  procmon_service_->SetPid(getpid());

  pthread_t tid;
  ASSERT_EQ(0, pthread_create(&tid, nullptr, &TestShutdownThread, hal_));

  // Call and validate results. Run() will not return any error.
  FLAGS_warmboot = false;
  ASSERT_OK(hal_->Run());  // blocking until HandleSignal() is called in
                           // TestShutdownThread()
  ASSERT_EQ(0, pthread_join(tid, nullptr));
}

TEST_F(HalTest, StartAndShutdownServerWhenProcmonCheckinFails) {
  EXPECT_CALL(*switch_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*auth_policy_checker_mock_, Shutdown())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*credentials_manager_mock_,
              GenerateExternalFacingServerCredentials())
      .WillOnce(Return(::grpc::InsecureServerCredentials()));
  procmon_service_->SetPid(getpid() + 1);

  pthread_t tid;
  ASSERT_EQ(0, pthread_create(&tid, nullptr, &TestShutdownThread, hal_));

  // Call and validate results. Even if Checkin is false, we still do not return
  // any error. We just log an error.
  FLAGS_warmboot = false;
  ASSERT_OK(hal_->Run());  // blocking until HandleSignal() is called in
                           // TestShutdownThread()
  ASSERT_EQ(0, pthread_join(tid, nullptr));
}

}  // namespace hal
}  // namespace stratum
