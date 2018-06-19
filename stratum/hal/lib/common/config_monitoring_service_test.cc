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


#include "stratum/hal/lib/common/config_monitoring_service.h"

#include <grpcpp/grpcpp.h>
#include <memory>

#include "gflags/gflags.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/common/mock_gnmi_publisher.h"
#include "stratum/hal/lib/common/mock_subscribe_reader_writer.h"
#include "stratum/hal/lib/common/switch_mock.h"
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include <google/protobuf/text_format.h>

DECLARE_string(chassis_config_file);
DECLARE_string(test_tmpdir);

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::WithArgs;

namespace stratum {
namespace hal {

class Event;

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

class ConfigMonitoringServiceTest
    : public ::testing::TestWithParam<OperationMode> {
 protected:
  void SetUp() override {
    FLAGS_chassis_config_file = FLAGS_test_tmpdir + "/config.pb.txt";
    mode_ = GetParam();
    switch_mock_ = absl::make_unique<SwitchMock>();
    auth_policy_checker_mock_ = absl::make_unique<AuthPolicyCheckerMock>();
    error_buffer_ = absl::make_unique<ErrorBuffer>();
    config_monitoring_service_ = absl::make_unique<ConfigMonitoringService>(
        mode_, switch_mock_.get(), auth_policy_checker_mock_.get(),
        error_buffer_.get());
    gnmi_publisher_ =
        absl::make_unique<NiceMock<MockGnmiPublisher>>(switch_mock_.get());
  }

  void FillTestChassisConfigAndSave(ChassisConfig* config) {
    const std::string& config_text = absl::Substitute(
        kChassisConfigTemplate, kNodeId1, kUnit1 + 1, kNodeId2, kUnit2 + 1);
    ASSERT_OK(ParseProtoFromString(config_text, config));
    // Save the config text to file to emulate the case chassis comes up with
    // a saved config.
    ASSERT_OK(WriteStringToFile(config_text, FLAGS_chassis_config_file));
  }

  void CheckRunningChassisConfig(const ChassisConfig* config) {
    absl::ReaderMutexLock l(&config_monitoring_service_->config_lock_);
    if (config == nullptr) {
      ASSERT_TRUE(config_monitoring_service_->running_chassis_config_ ==
                  nullptr);
    } else {
      ASSERT_TRUE(config_monitoring_service_->running_chassis_config_ !=
                  nullptr);
      EXPECT_TRUE(ProtoEqual(
          *config, *config_monitoring_service_->running_chassis_config_));
    }
  }

  // A proxy to private method of ConfigMonitoringService class.
  ::grpc::Status DoSubscribe(::grpc::ServerContext* context,
                             ServerSubscribeReaderWriterInterface* stream) {
    return config_monitoring_service_->DoSubscribe(gnmi_publisher_.get(),
                                                   context, stream);
  }

  // A proxy to private method of ConfigMonitoringService class.
  ::grpc::Status DoGet(::grpc::ServerContext* context,
                       const ::gnmi::GetRequest* req,
                       ::gnmi::GetResponse* resp) {
    return config_monitoring_service_->DoGet(context, req, resp);
  }

  static constexpr char kChassisConfigTemplate[] = R"PROTO(
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
      singleton_ports {
        id: 1
        name: "device1.domain.net.com:ce-1/1"
        slot: 1
        port: 1
        speed_bps: 100000000000
      }
      singleton_ports {
        id: 2
        name: "device1.domain.net.com:ce-1/2"
        slot: 1
        port: 2
        speed_bps: 100000000000
      }
  )PROTO";
  static constexpr char kErrorMsg[] = "Some error";
  static constexpr uint64 kNodeId1 = 123123123;
  static constexpr uint64 kNodeId2 = 456456456;
  static constexpr int kUnit1 = 0;
  static constexpr int kUnit2 = 1;
  OperationMode mode_;
  std::unique_ptr<ConfigMonitoringService> config_monitoring_service_;
  std::unique_ptr<SwitchMock> switch_mock_;
  std::unique_ptr<AuthPolicyCheckerMock> auth_policy_checker_mock_;
  std::unique_ptr<ErrorBuffer> error_buffer_;
  std::unique_ptr<NiceMock<MockGnmiPublisher>> gnmi_publisher_;
};

constexpr char ConfigMonitoringServiceTest::kChassisConfigTemplate[];
constexpr char ConfigMonitoringServiceTest::kErrorMsg[];
constexpr uint64 ConfigMonitoringServiceTest::kNodeId1;
constexpr uint64 ConfigMonitoringServiceTest::kNodeId2;
constexpr int ConfigMonitoringServiceTest::kUnit1;
constexpr int ConfigMonitoringServiceTest::kUnit2;

TEST_P(ConfigMonitoringServiceTest,
       ColdbootSetupWontPushSavedConfigInCoupledMode) {
  if (mode_ == OPERATION_MODE_STANDALONE || mode_ == OPERATION_MODE_SIM) return;

  // Setup the test config and also save it to the file.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);

  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  ASSERT_OK(config_monitoring_service_->Setup(false));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());
  CheckRunningChassisConfig(nullptr);
}

TEST_P(ConfigMonitoringServiceTest, ColdbootSetupSuccessForSavedConfig) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Setup the test config and also save it to the file.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);

  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  ASSERT_OK(config_monitoring_service_->Setup(false));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());
  CheckRunningChassisConfig(&config);
}

TEST_P(ConfigMonitoringServiceTest, ColdbootSuccessForNoSavedConfig) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Delete the saved config. There will be no config push.
  if (PathExists(FLAGS_chassis_config_file)) {
    ASSERT_OK(RemoveFile(FLAGS_chassis_config_file));
  }

  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  ASSERT_OK(config_monitoring_service_->Setup(false));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());
  CheckRunningChassisConfig(nullptr);
}

TEST_P(ConfigMonitoringServiceTest,
       ColdbootSetupFailureWhenRegisterEventNotifyWriterFails) {
  // Setup the test config and also save it to the file.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);

  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));

  // Call and validate results.
  ::util::Status status = config_monitoring_service_->Setup(false);
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
  const auto& errors = error_buffer_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg));
  EXPECT_THAT(errors[0].error_message(), HasSubstr("gNMI notification"));
  CheckRunningChassisConfig(nullptr);
}

TEST_P(ConfigMonitoringServiceTest, ColdbootSetupFailureWhenPushFails) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Setup the test config and also save it to the file.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);

  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));

  // Call and validate results.
  ::util::Status status = config_monitoring_service_->Setup(false);
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
  const auto& errors = error_buffer_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg));
  EXPECT_THAT(errors[0].error_message(), HasSubstr("saved chassis config"));
  CheckRunningChassisConfig(nullptr);
}

TEST_P(ConfigMonitoringServiceTest, WarmbootSetupSuccessForSavedConfig) {
  // Setup the test config and also save it to the file. In case of warmboot
  // we read the file but we dont push anything to hardware.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);

  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  ASSERT_OK(config_monitoring_service_->Setup(true));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());
  CheckRunningChassisConfig(&config);
}

TEST_P(ConfigMonitoringServiceTest, WarmbootSetupFailureForNoSavedConfig) {
  // Delete the saved config. There will be no config push.
  if (PathExists(FLAGS_chassis_config_file)) {
    ASSERT_OK(RemoveFile(FLAGS_chassis_config_file));
  }

  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  ::util::Status status = config_monitoring_service_->Setup(true);
  ASSERT_EQ(ERR_FILE_NOT_FOUND, status.error_code());
  const auto& errors = error_buffer_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(),
              HasSubstr("not read saved chassis config"));
  CheckRunningChassisConfig(nullptr);
}

TEST_P(ConfigMonitoringServiceTest, WarmbootSetupFailureForBadSavedConfig) {
  // Write some invalid data to FLAGS_chassis_config_file so that the parsing
  // fails.
  ASSERT_OK(WriteStringToFile("blah blah", FLAGS_chassis_config_file));

  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results.
  ::util::Status status = config_monitoring_service_->Setup(true);
  ASSERT_EQ(ERR_INTERNAL, status.error_code());
  const auto& errors = error_buffer_->GetErrors();
  ASSERT_EQ(1U, errors.size());
  EXPECT_THAT(errors[0].error_message(),
              HasSubstr("not read saved chassis config"));
  CheckRunningChassisConfig(nullptr);
}

TEST_P(ConfigMonitoringServiceTest, SetupAndThenTeardownSuccess) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Setup the test config and also save it to the file.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);

  EXPECT_CALL(*switch_mock_, RegisterEventNotifyWriter(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*switch_mock_, PushChassisConfig(EqualsProto(config)))
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results for setup.
  ASSERT_OK(config_monitoring_service_->Setup(false));
  CheckRunningChassisConfig(&config);

  EXPECT_CALL(*switch_mock_, UnregisterEventNotifyWriter())
      .WillOnce(Return(::util::OkStatus()));

  // Call and validate results for teardown.
  ASSERT_OK(config_monitoring_service_->Teardown());
  CheckRunningChassisConfig(nullptr);
}

TEST_P(ConfigMonitoringServiceTest, SubscribeExistingPathSuccess) {
  MockServerReaderWriter stream;
  ::grpc::ServerContext context;

  // Build a stream subscription request for subtree that is supported.
  ::gnmi::SubscribeRequest req;
  constexpr char kReq[] = R"PROTO(
  subscribe {
    mode: STREAM
    subscription {
      path {
        elem { name: "interfaces" }
        elem { name: "interface" key { key: "name" value: "*" } }
      }
      mode: SAMPLE
      sample_interval: 1
    }
  }
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Two Read() calls are expected:
  // - first simulates reception of the original subscribe message.
  // - second simulates closure of the connection.
  EXPECT_CALL(stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(req), Return(true)))
      .WillOnce(Return(false));

  // Simulate path being found.
  EXPECT_CALL(*gnmi_publisher_, SubscribePeriodic(_, _, _, _))
      .WillOnce(Return(::util::OkStatus()));

  // Actual test. Simulates reception of a Subscribe gRPC call.
  EXPECT_TRUE(DoSubscribe(&context, &stream).ok());
}

TEST_P(ConfigMonitoringServiceTest, SubscribeExistingPathFail) {
  MockServerReaderWriter stream;
  ::grpc::ServerContext context;

  // Build a stream subscription request for subtree that is not supported.
  ::gnmi::SubscribeRequest req;
  constexpr char kReq[] = R"PROTO(
  subscribe {
    mode: STREAM
    subscription {
      path {
        elem { name: "blah" }
      }
      mode: SAMPLE
      sample_interval: 1
    }
  }
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Two Read() calls are expected:
  // - first simulates reception of the original subscribe message.
  // - second simulates closure of the connection.
  EXPECT_CALL(stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(req), Return(true)))
      .WillOnce(Return(false));

  // Simulate path not being found.
  ::util::Status error = MAKE_ERROR(ERR_INVALID_PARAM) << "path not supported.";
  EXPECT_CALL(*gnmi_publisher_, SubscribePeriodic(_, _, _, _))
      .WillOnce(Return(error));

  // Invalid subscription request triggers one response, therefore one call to
  // Write() is expected. The message written into the stream is saved to local
  // variable 'resp' for further examination.
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(DoAll(SaveArg<0>(&resp), Return(true)));

  // Actual test. Simulates reception of a Subscribe gRPC call.
  EXPECT_TRUE(DoSubscribe(&context, &stream).ok());

  // The response should include an error message!
  ASSERT_TRUE(resp.has_error());
}

TEST_P(ConfigMonitoringServiceTest, SubscribeExistingPathPassFail) {
  MockServerReaderWriter stream;
  ::grpc::ServerContext context;

  // Build a stream subscription request for subtree that is not supported.
  ::gnmi::SubscribeRequest req;
  constexpr char kReq[] = R"PROTO(
  subscribe {
    mode: STREAM
    subscription {
      path {
        elem { name: "interfaces" }
        elem { name: "interface" key { key: "name" value: "*" } }
      }
      mode: SAMPLE
      sample_interval: 1
    }
    subscription {
      path {
        elem { name: "blah" }
      }
      mode: SAMPLE
      sample_interval: 1
    }
  }
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Two Read() calls are expected:
  // - first simulates reception of the original subscribe message.
  // - second simulates closure of the connection.
  EXPECT_CALL(stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(req), Return(true)))
      .WillOnce(Return(false));

  // Simulate path not being found.
  ::util::Status error = MAKE_ERROR(ERR_INVALID_PARAM) << "path not supported.";
  EXPECT_CALL(*gnmi_publisher_, SubscribePeriodic(_, _, _, _))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(error));

  // Invalid subscription request triggers one response, therefore one call to
  // Write() is expected. The message written into the stream is saved to local
  // variable 'resp' for further examination.
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(DoAll(SaveArg<0>(&resp), Return(true)));

  // Actual test. Simulates reception of a Subscribe gRPC call.
  EXPECT_TRUE(DoSubscribe(&context, &stream).ok());

  // The response should include an error message!
  ASSERT_TRUE(resp.has_error());
}

TEST_P(ConfigMonitoringServiceTest, SubscribeAndPollSuccess) {
  MockServerReaderWriter stream;
  ::grpc::ServerContext context;

  // Build a poll subscription request for subtree that is supported.
  ::gnmi::SubscribeRequest req1;
  constexpr char kReq1[] = R"PROTO(
  subscribe {
    mode: POLL
    subscription {
      path {
        elem { name: "interfaces" }
        elem { name: "interface" key { key: "name" value: "*" } }
      }
    }
  }
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq1, &req1))
      << "Failed to parse proto from the following string: " << kReq1;

  // Build actual poll request.
  ::gnmi::SubscribeRequest req2;
  constexpr char kReq2[] = R"PROTO(
  poll {
  }
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq2, &req2))
      << "Failed to parse proto from the following string: " << kReq2;

  // Three Read() calls are expected:
  // - first simulates reception of the original subscribe message.
  // - second simulates reception of the poll request message.
  // - third simulates closure of the connection.
  EXPECT_CALL(stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(req1), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(req2), Return(true)))
      .WillOnce(Return(false));

  // Simulate path being found.
  EXPECT_CALL(*gnmi_publisher_, SubscribePoll(_, _, _))
      .WillOnce(Return(::util::OkStatus()));

  // Simulate successful poll operation.
  EXPECT_CALL(*gnmi_publisher_, HandlePoll(_))
      .WillOnce(Return(::util::OkStatus()));

  // Actual test. Simulates reception of a Subscribe gRPC call.
  EXPECT_TRUE(DoSubscribe(&context, &stream).ok());
}

TEST_P(ConfigMonitoringServiceTest, DoubleSubscribeFail) {
  MockServerReaderWriter stream;
  ::grpc::ServerContext context;

  // Build a stream subscription request for subtree that is supported.
  ::gnmi::SubscribeRequest req;
  constexpr char kReq[] = R"PROTO(
  subscribe {
    mode: STREAM
    subscription {
      path {
        elem { name: "interfaces" }
        elem { name: "interface" key { key: "name" value: "*" } }
      }
      mode: SAMPLE
      sample_interval: 1
    }
  }
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Three Read() calls are expected:
  // - first simulates reception of the original subscribe message.
  // - second simulates reception of the additional invalid subscribe message.
  // - third simulates closure of the connection.
  EXPECT_CALL(stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(req), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(req), Return(true)))
      .WillOnce(Return(false));

  // An invalid request that results in an error triggers one response,
  // therefore one call to Write() is expected. The message written into
  // the stream is saved to local  variable 'resp' for further examination.
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(DoAll(SaveArg<0>(&resp), Return(true)));

  // Simulate path being found.
  EXPECT_CALL(*gnmi_publisher_, SubscribePeriodic(_, _, _, _))
      .WillOnce(Return(::util::OkStatus()));

  // Actual test. Simulates reception of a Subscribe gRPC call.
  EXPECT_TRUE(DoSubscribe(&context, &stream).ok());

  // The response should include an error message!
  ASSERT_TRUE(resp.has_error());
}

TEST_P(ConfigMonitoringServiceTest, DuplicateSubscribeFail) {
  MockServerReaderWriter stream;
  ::grpc::ServerContext context;

  // Build a stream subscription request for subtree that is supported.
  // Add another request for the same path. This is illeagal combination.
  ::gnmi::SubscribeRequest req;
  constexpr char kReq[] = R"PROTO(
  subscribe {
    mode: STREAM
    subscription {
      path {
        elem { name: "interfaces" }
        elem { name: "interface" key { key: "name" value: "*" } }
      }
      mode: SAMPLE
      sample_interval: 1
    }
    subscription {
      path {
        elem { name: "interfaces" }
        elem { name: "interface" key { key: "name" value: "*" } }
      }
      mode: SAMPLE
      sample_interval: 1
    }
  }
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Two Read() calls are expected:
  // - first simulates reception of the original subscribe message.
  // - second simulates closure of the connection.
  EXPECT_CALL(stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(req), Return(true)))
      .WillOnce(Return(false));

  // An invalid request that results in an error triggers one response,
  // therefore one call to Write() is expected. The message written into
  // the stream is saved to local  variable 'resp' for further examination.
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(DoAll(SaveArg<0>(&resp), Return(true)));

  // Simulate path being found.
  EXPECT_CALL(*gnmi_publisher_, SubscribePeriodic(_, _, _, _))
      .WillOnce(Return(::util::OkStatus()));

  // Configure the device - the model will reconfigure itself to reflect the
  // configuration.
  ChassisConfig hal_config;
  FillTestChassisConfigAndSave(&hal_config);
  ASSERT_OK(
      gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config)));

  // Actual test. Simulates reception of a Subscribe gRPC call.
  EXPECT_TRUE(DoSubscribe(&context, &stream).ok());

  // The response should include an error message!
  ASSERT_TRUE(resp.has_error());
}

TEST_P(ConfigMonitoringServiceTest, SubscribeOnChangeWithInitialValueSuccess) {
  MockServerReaderWriter stream;
  ::grpc::ServerContext context;

  // Build a on_change subscription request for subtree that is supported.
  ::gnmi::SubscribeRequest req;
  constexpr char kReq[] = R"PROTO(
  subscribe {
    mode: STREAM
    subscription {
      path {
        elem { name: "interfaces" }
        elem { name: "interface" key { key: "name" value: "*" } }
      }
      mode: ON_CHANGE
   }
  }
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Two Read() calls are expected:
  // - first simulates reception of the original subscribe message.
  // - second simulates closure of the connection.
  EXPECT_CALL(stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(req), Return(true)))
      .WillOnce(Return(false));

  // One Write() call is expected: the sync_response message.
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(DoAll(SaveArg<0>(&resp), Return(true)));

  // Simulate path being found.
  EXPECT_CALL(*gnmi_publisher_, SubscribeOnChange(_, _, _))
      .WillOnce(Return(::util::OkStatus()));

  // Simulate successful initial value poll operation.
  EXPECT_CALL(*gnmi_publisher_, SubscribePoll(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*gnmi_publisher_, HandlePoll(_))
      .WillOnce(Return(::util::OkStatus()));

  // Actual test. Simulates reception of a Subscribe gRPC call.
  EXPECT_TRUE(DoSubscribe(&context, &stream).ok());

  // Check if the response mesage has been sent.
  EXPECT_TRUE(resp.sync_response());
}

TEST_P(ConfigMonitoringServiceTest, CheckConvertTargetDefinedToOnChange) {
  MockServerReaderWriter stream;
  ::grpc::ServerContext context;

  // One of the subscription modes, TARGET_DEFINED, leaves the decision how to
  // treat the received subscription request to the switch.
  // GnmiPublisher::UpdateSubscritionWithTargetSpecficModeSpecification() method
  // modifies the 'subscription' request to be what the switch would like it to
  // be.
  // This test test this functionality.

  // Build a TARGET_DEFINED subscription request for subtree that is supported.
  // This subscription request will be changed into an ON_CHANGE subscription
  // request.
  ::gnmi::SubscribeRequest req;
  constexpr char kReq[] = R"PROTO(
  subscribe {
    mode: STREAM
    subscription {
      path {
        elem { name: "interfaces" }
        elem { name: "interface" key { key: "name" value: "*" } }
      }
      mode: TARGET_DEFINED
   }
  }
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // The 'heart' of the test.
  // These two functions mock a successful conversion to an ON_CHANGE request.
  EXPECT_CALL(*gnmi_publisher_,
              UpdateSubscriptionWithTargetSpecificModeSpecification(_, _))
      .WillOnce(DoAll(WithArgs<1>(Invoke([](::gnmi::Subscription* req) {
                        req->set_mode(::gnmi::SubscriptionMode::ON_CHANGE);
                      })),
                      Return(::util::OkStatus())));
  EXPECT_CALL(*gnmi_publisher_, SubscribeOnChange(_, _, _))
      .WillOnce(Return(::util::OkStatus()));

  // Boilerplate code needed to execute the test scenario. They simulate the
  // sequence of events that is defined by the gNMI specification when a
  // TARGET_DEFINED subscription request is received and changed into a
  // ON_CHANGE subscription request.

  // Two Read() calls are expected:
  // - first simulates reception of the original subscribe message.
  // - second simulates closure of the connection.
  EXPECT_CALL(stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(req), Return(true)))
      .WillOnce(Return(false));

  // One Write() call is expected: the sync_response message.
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(DoAll(SaveArg<0>(&resp), Return(true)));

  // Simulate successful initial value poll operation.
  EXPECT_CALL(*gnmi_publisher_, SubscribePoll(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*gnmi_publisher_, HandlePoll(_))
      .WillOnce(Return(::util::OkStatus()));

  // Make sure that only the ON_CHANGE subcription is called.
  EXPECT_CALL(*gnmi_publisher_, SubscribePeriodic(_, _, _, _)).Times(0);

  // Triggering of the test scenario. Simulates reception of a Subscribe gRPC
  // call.
  EXPECT_TRUE(DoSubscribe(&context, &stream).ok());

  // Check if the response message has been sent.
  EXPECT_TRUE(resp.sync_response());
}

// DoGet() should fail if executed before a config is pushed.
TEST_P(ConfigMonitoringServiceTest, GnmiGetRootConfigBeforePush) {
  // Prepare a GET request.
  ::gnmi::GetRequest req;
  constexpr char kReq[] = R"PROTO(
  path {
    elem { name: "/" }
  }
  type: CONFIG
  encoding: PROTO
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Run the method that processes the GET request.
  ::grpc::ServerContext context;
  ::gnmi::GetResponse resp;
  EXPECT_FALSE(DoGet(&context, &req, &resp).ok());
}

// DoGet() should fail if the request is not for CONFIG nodes of the whole tree.
TEST_P(ConfigMonitoringServiceTest, GnmiGetRootNonConfig) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Prepare and push configuration. The method under test requires the
  // configuration to be pushed.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);
  ASSERT_OK(config_monitoring_service_->Setup(false));

  // Prepare a GET request.
  ::gnmi::GetRequest req;
  constexpr char kReq[] = R"PROTO(
  path {
    elem { name: "/" }
  }
  type: STATE
  encoding: PROTO
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Run the method that processes the GET request.
  ::grpc::ServerContext context;
  ::gnmi::GetResponse resp;
  EXPECT_FALSE(DoGet(&context, &req, &resp).ok());
}

// DoGet() should fail if the requested encoding is not PROTO
TEST_P(ConfigMonitoringServiceTest, GnmiGetRootNonProto) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Prepare and push configuration. The method under test requires the
  // configuration to be pushed.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);
  ASSERT_OK(config_monitoring_service_->Setup(false));

  // Prepare a GET request.
  ::gnmi::GetRequest req;
  constexpr char kReq[] = R"PROTO(
  path {
    elem { name: "/" }
  }
  type: CONFIG
  encoding: JSON
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Run the method that processes the GET request.
  ::grpc::ServerContext context;
  ::gnmi::GetResponse resp;
  EXPECT_FALSE(DoGet(&context, &req, &resp).ok());
}

// Successful DoGet() execution for whole config tree.
TEST_P(ConfigMonitoringServiceTest, GnmiGetRootConfig) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Prepare and push configuration. The method under test requires the
  // configuration to be pushed.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);
  ASSERT_OK(config_monitoring_service_->Setup(false));

  // Prepare a GET request.
  ::gnmi::GetRequest req;
  constexpr char kReq[] = R"PROTO(
  path {
    elem { name: "/" }
  }
  type: CONFIG
  encoding: PROTO
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Run the method that processes the GET request.
  ::grpc::ServerContext context;
  ::gnmi::GetResponse resp;
  auto grpc_status = DoGet(&context, &req, &resp);
  EXPECT_TRUE(grpc_status.ok()) << grpc_status.error_message();

  EXPECT_TRUE(resp.notification(0).update(0).path() == GetPath("/")());
}

// DoGet() should fail if requested to handle not-existent path.
TEST_P(ConfigMonitoringServiceTest, GnmiGetBlah) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Prepare and push configuration. The method under test requires the
  // configuration to be pushed.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);
  ASSERT_OK(config_monitoring_service_->Setup(false));

  // Prepare a GET request.
  ::gnmi::GetRequest req;
  constexpr char kReq[] = R"PROTO(
  path {
    elem { name: "interfaces" }
    elem { name: "interface"
           key { key: "name" value: "device1.domain.net.com:ce-1/2" }
         }
    elem { name: "state" }
    elem { name: "blah" }
  }
  type: CONFIG
  encoding: PROTO
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Run the method that processes the GET request.
  ::grpc::ServerContext context;
  ::gnmi::GetResponse resp;
  EXPECT_FALSE(DoGet(&context, &req, &resp).ok());
}

// Successful DoGet() execution for simple leaf.
TEST_P(ConfigMonitoringServiceTest,
       GnmiGetInterfacesInterfaceStateAdminStatus) {
  if (mode_ == OPERATION_MODE_COUPLED) return;

  // Prepare and push configuration. The method under test requires the
  // configuration to be pushed.
  ChassisConfig config;
  FillTestChassisConfigAndSave(&config);
  ASSERT_OK(config_monitoring_service_->Setup(false));

  // Prepare a GET request.
  ::gnmi::GetRequest req;
  constexpr char kReq[] = R"PROTO(
  path {
    elem { name: "interfaces" }
    elem { name: "interface"
           key { key: "name" value: "device1.domain.net.com:ce-1/2" }
         }
    elem { name: "state" }
    elem { name: "admin-status" }
  }
  type: CONFIG
  encoding: PROTO
  )PROTO";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReq, &req))
      << "Failed to parse proto from the following string: " << kReq;

  // Run the method that processes the GET request.
  ::grpc::ServerContext context;
  ::gnmi::GetResponse resp;
  auto grpc_status = DoGet(&context, &req, &resp);
  EXPECT_TRUE(grpc_status.ok()) << grpc_status.error_message();

  EXPECT_TRUE(
      resp.notification(0).update(0).path() ==
      GetPath("interfaces")("interface", "device1.domain.net.com:ce-1/2")(
          "state")("admin-status")());
}

// TODO: Finish the unit testing.

INSTANTIATE_TEST_CASE_P(ConfigMonitoringServiceTestWithMode,
                        ConfigMonitoringServiceTest,
                        ::testing::Values(OPERATION_MODE_STANDALONE,
                                          OPERATION_MODE_COUPLED,
                                          OPERATION_MODE_SIM));

}  // namespace hal
}  // namespace stratum
