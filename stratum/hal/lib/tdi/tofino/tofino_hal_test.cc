// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// adapted from ipdk/ipdk_hal_test.cc, which was
// adapted from common/hal_test.cc

#include "stratum/hal/lib/tdi/tofino/tofino_hal.h"

#include "absl/strings/str_join.h"
#include "absl/strings/substitute.h"
#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/switch_mock.h"
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

DECLARE_string(external_stratum_urls);
DECLARE_bool(warmboot);
DECLARE_string(chassis_config_file);
DECLARE_string(forwarding_pipeline_configs_file);
DECLARE_string(test_tmpdir);
DECLARE_string(local_stratum_url);
DECLARE_string(persistent_config_dir);

namespace stratum {
namespace hal {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

class TofinoHalTest : public ::testing::Test {
 protected:
  static const std::string RandomURL() {
    // Every call to PickUnusedPortOrDie() will return a new port number.
    return "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
  }

  // Per-test-case set-up.
  static void SetUpTestCase() {
    // Set up TofinoHal class instance under test.
    switch_mock_ = new ::testing::StrictMock<SwitchMock>();
    auth_policy_checker_mock_ =
        new ::testing::StrictMock<AuthPolicyCheckerMock>();
    hal_ = TofinoHal::CreateSingleton(
	kMode, switch_mock_, auth_policy_checker_mock_);
    ASSERT_NE(hal_, nullptr);
  }

  // Per-test-case tear-down.
  static void TearDownTestCase() {
    delete switch_mock_;
    delete auth_policy_checker_mock_;
    switch_mock_ = nullptr;
    auth_policy_checker_mock_ = nullptr;
  }

  // Per-test setup.
  void SetUp() override {
    FLAGS_chassis_config_file = FLAGS_test_tmpdir + "/chassis_config.pb.txt";
    FLAGS_forwarding_pipeline_configs_file =
        FLAGS_test_tmpdir + "/forwarding_pipeline_configs_file.pb.txt";
    FLAGS_persistent_config_dir = FLAGS_test_tmpdir + "/config_dir";
    FLAGS_external_stratum_urls =
        absl::StrJoin({RandomURL(), RandomURL()}, ",");
    FLAGS_local_stratum_url = RandomURL();
    // FLAGS_cmal_service_url = RandomURL();  // google only
    ASSERT_OK(hal_->SanityCheck());
    hal_->ClearErrors();
  }

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
  static TofinoHal* hal_;  // points to the singleton instance
};

constexpr char TofinoHalTest::kChassisConfigTemplate[];
constexpr char TofinoHalTest::kForwardingPipelineConfigsTemplate[];
constexpr char TofinoHalTest::kErrorMsg[];
constexpr uint64 TofinoHalTest::kNodeId1;
constexpr uint64 TofinoHalTest::kNodeId2;
constexpr int TofinoHalTest::kUnit1;
constexpr int TofinoHalTest::kUnit2;
constexpr OperationMode TofinoHalTest::kMode;

::testing::StrictMock<SwitchMock>* TofinoHalTest::switch_mock_ = nullptr;
::testing::StrictMock<AuthPolicyCheckerMock>*
    TofinoHalTest::auth_policy_checker_mock_ = nullptr;
TofinoHal* TofinoHalTest::hal_ = nullptr;

TEST_F(TofinoHalTest, SanityCheckFailureWhenExtURLsNotGiven) {
  FLAGS_external_stratum_urls = "";
  ::util::Status status = hal_->SanityCheck();
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("No external URLs were specified"));
}

TEST_F(TofinoHalTest, SanityCheckFailureWhenExtURLsAreInvalid) {
  const auto& url = RandomURL();
  FLAGS_external_stratum_urls = absl::StrJoin({url, std::string("blah")}, ",");
  FLAGS_local_stratum_url = url;
  ::util::Status status = hal_->SanityCheck();
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("reserved local URLs as an external URL"));
}

TEST_F(TofinoHalTest, SanityCheckFailureWhenPersistentConfigDirFlagNotGiven) {
  FLAGS_persistent_config_dir = "";
  ::util::Status status = hal_->SanityCheck();
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr("persistent_config_dir flag needs to be explicitly given"));
}

TEST_F(TofinoHalTest, ColdbootSetupSuccessForSavedConfigs) {
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

TEST_F(TofinoHalTest, ColdbootSetupSuccessForNoSavedConfigAtAll) {
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

TEST_F(TofinoHalTest, ColdbootSetupSuccessForNoForwardingPipelineConfig) {
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

TEST_F(TofinoHalTest, ColdbootSetupSuccessForNoChassisConfig) {
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

TEST_F(TofinoHalTest, ColdbootSetupFailureWhenChassisConfigPushFails) {
  // Setup and save the test config(s).
  ChassisConfig chassis_config;
  ForwardingPipelineConfigs forwarding_pipeline_configs;
  FillTestChassisConfigAndSave(&chassis_config);
  FillTestForwardingPipelineConfigsAndSave(&forwarding_pipeline_configs);

  EXPECT_CALL(*switch_mock_, PushChassisConfig(EqualsProto(chassis_config)))
      .WillOnce(
          Return(::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));

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

TEST_F(TofinoHalTest, ColdbootSetupFailureWhenPipelineConfigPushFailsForSomeNodes) {
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
      .WillOnce(
          Return(::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));
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

TEST_F(TofinoHalTest, WarmbootSetupSuccessForSavedConfig) {
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

TEST_F(TofinoHalTest, WarmbootbootSetupFailureForNoSavedConfig) {
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

TEST_F(TofinoHalTest, WarmbootSetupFailureWhenUnfreezeFails) {
  // Setup and save the test config(s).
  ChassisConfig chassis_config;
  ForwardingPipelineConfigs forwarding_pipeline_configs;
  FillTestChassisConfigAndSave(&chassis_config);
  FillTestForwardingPipelineConfigsAndSave(&forwarding_pipeline_configs);

  EXPECT_CALL(*switch_mock_, Unfreeze())
      .WillOnce(
          Return(::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));

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

TEST_F(TofinoHalTest, ColdbootTeardownSuccess) {
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

TEST_F(TofinoHalTest, ColdbootTeardownFailureWhenSwitchInterfaceShutdownFails) {
  EXPECT_CALL(*switch_mock_, Shutdown())
      .WillOnce(
          Return(::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));
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
  EXPECT_THAT(errors[0].error_message(), HasSubstr("shut down"));
}

TEST_F(TofinoHalTest, ColdbootTeardownFailureWhenAuthPolicyCheckerShutdownFails) {
  EXPECT_CALL(*switch_mock_, Shutdown()).WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*auth_policy_checker_mock_, Shutdown())
      .WillOnce(
          Return(::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));

  // Call and validate results.
  FLAGS_warmboot = false;
  ::util::Status status = hal_->Teardown();
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
  const auto& errors = hal_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg));
  EXPECT_THAT(errors[0].error_message(), HasSubstr("shut down"));
}

TEST_F(TofinoHalTest, WarmbootTeardownSuccess) {
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

TEST_F(TofinoHalTest, WarmbootTeardownFailure) {
  EXPECT_CALL(*switch_mock_, Shutdown())
      .WillOnce(
          Return(::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));
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
  EXPECT_THAT(errors[0].error_message(), HasSubstr("shut down"));
}

namespace {

void* TestShutdownThread(void* arg) {
  sleep(3);  // some sleep to emulate a task.
  static_cast<TofinoHal*>(arg)->HandleSignal(SIGINT);
  return nullptr;
}

}  // namespace

}  // namespace hal
}  // namespace stratum
