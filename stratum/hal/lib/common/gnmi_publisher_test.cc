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


#include "stratum/hal/lib/common/gnmi_publisher.h"

#include "github.com/openconfig/gnmi/proto/gnmi/gnmi.pb.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/subscribe_reader_writer_mock.h"
#include "stratum/hal/lib/common/switch_mock.h"
#include "stratum/lib/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Not;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;

namespace stratum {
namespace hal {

// There are two types of tests in this file, namely: ones that can be executed
// multiple times with different paths and ones that should be executed once. To
// avoid duplication of the helper methods and setup/teardown code a base class
// has been created that is the n used to create two classes that are then used
// to execute the tests.
class SubscriptionTestBase {
 protected:
  SubscriptionTestBase() {
    gnmi_publisher_ = absl::make_unique<GnmiPublisher>(&switch_mock_);
    Init();
  }

  void Init() {
    GetSampleHalConfig();

    // Configure the device - the model will reconfigure itself to reflect the
    // configuration.
    ASSERT_OK(
        gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config_)));
  }

  virtual ~SubscriptionTestBase() {}

  void GetSampleHalConfig() {
    constexpr char kHalConfig[] = R"proto(
      description: "Sample Generic Tomahawk config with 2x100G ports."
      chassis { platform: PLT_GENERIC_TOMAHAWK name: "device1.domain.net.com" }
      nodes {
        id: 1
        name: "xy1switch.domain.net.com"
        slot: 1
        index: 1
        config_params {
          qos_config {
            traffic_class_mapping { internal_priority: 0 traffic_class: BE1 }
            traffic_class_mapping { internal_priority: 1 traffic_class: AF1 }
            traffic_class_mapping { internal_priority: 2 traffic_class: AF2 }
            cosq_mapping { internal_priority: 2 q_num: 0 }
            cosq_mapping { internal_priority: 1 q_num: 1 }
            cosq_mapping { internal_priority: 0 q_num: 2 }
          }
        }
      }
      singleton_ports {
        id: 1
        name: "device1.domain.net.com:ce-1/1"
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 1
      }
      singleton_ports {
        id: 2
        name: "device1.domain.net.com:ce-1/2"
        slot: 1
        port: 2
        speed_bps: 100000000000
        node: 1
      })proto";
    ASSERT_OK(ParseProtoFromString(kHalConfig, &hal_config_));
  }

  void PrintNodeWithOnTimer() {
    absl::WriterMutexLock l(&gnmi_publisher_->access_lock_);
    PrintNodeWithOnTimer(*gnmi_publisher_->parse_tree_.GetRoot(), "");
  }

  void PrintNodeWithOnTimer(const TreeNode& node, const std::string& prefix) {
    LOG(ERROR) << prefix << node.name() << ": "
               << node.AllSubtreeLeavesSupportOnTimer() << " "
               << node.supports_on_timer_;
    for (const auto& entry : node.children_) {
      PrintNodeWithOnTimer(entry.second, prefix + " ");
    }
  }

  void PrintNodeWithOnChange() {
    absl::WriterMutexLock l(&gnmi_publisher_->access_lock_);
    PrintNodeWithOnChange(*gnmi_publisher_->parse_tree_.GetRoot(), "");
  }

  void PrintNodeWithOnChange(const TreeNode& node, const std::string& prefix) {
    LOG(ERROR) << prefix << node.name() << ": "
               << node.AllSubtreeLeavesSupportOnChange() << " "
               << node.supports_on_timer_;
    for (const auto& entry : node.children_) {
      PrintNodeWithOnChange(entry.second, prefix + " ");
    }
  }

  void PrintPath(const ::gnmi::Path& path) {
    LOG(INFO) << path.ShortDebugString();
  }

  ChassisConfig hal_config_;
  SwitchMock switch_mock_;
  std::unique_ptr<GnmiPublisher> gnmi_publisher_;
};

// Tests to be executed only once.
class SubscriptionTest : public SubscriptionTestBase, public ::testing::Test {};

TEST_F(SubscriptionTest, SubscribeForSupportedPath) {
  SubscribeReaderWriterMock stream;

  SubscriptionHandle h;
  ::gnmi::Path path = GetPath("interfaces")("interface", "*")();
  EXPECT_OK(gnmi_publisher_->SubscribeOnChange(path, &stream, &h));
  PrintNodeWithOnChange();
}

TEST_F(SubscriptionTest, SubscribeForSupportedPathNullStream) {
  // Configure the device - the model will reconfigure itself to reflect the
  // configuration.
  ASSERT_OK(
      gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config_)));

  SubscriptionHandle h;
  ::gnmi::Path path = GetPath("interfaces")();
  EXPECT_THAT(
      gnmi_publisher_->SubscribePeriodic(Periodic(1000), path, nullptr, &h)
          .ToString(),
      HasSubstr("null"));
}

TEST_F(SubscriptionTest, SubscribeForSupportedPathNullHandle) {
  // Configure the device - the model will reconfigure itself to reflect the
  // configuration.
  ASSERT_OK(
      gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config_)));

  SubscribeReaderWriterMock stream;

  SubscriptionHandle h;
  ::gnmi::Path path = GetPath("interfaces")();
  EXPECT_THAT(
      gnmi_publisher_->SubscribePeriodic(Periodic(1000), path, &stream, nullptr)
          .ToString(),
      HasSubstr("null"));
}

TEST_F(SubscriptionTest, SubscribeForUnSupportedPath) {
  // Configure the device - the model will reconfigure itself to reflect the
  // configuration.
  ASSERT_OK(
      gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config_)));

  SubscribeReaderWriterMock stream;

  SubscriptionHandle h;
  ::gnmi::Path path = GetPath("blah")();
  EXPECT_THAT(
      gnmi_publisher_->SubscribePeriodic(Periodic(1000), path, &stream, &h)
          .ToString(),
      HasSubstr("unsupported"));
}

TEST_F(SubscriptionTest, SubscribeForEmptyPath) {
  // Configure the device - the model will reconfigure itself to reflect the
  // configuration.
  ASSERT_OK(
      gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config_)));

  SubscribeReaderWriterMock stream;

  SubscriptionHandle h;
  ::gnmi::Path path;
  EXPECT_THAT(
      gnmi_publisher_->SubscribePeriodic(Periodic(1000), path, &stream, &h)
          .ToString(),
      HasSubstr("empty"));
}

TEST_F(SubscriptionTest, HandleTimer) {
  // Configure the device - the model will reconfigure itself to reflect the
  // configuration.
  ASSERT_OK(
      gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config_)));
  PrintNodeWithOnTimer();

  SubscribeReaderWriterMock stream;

  SubscriptionHandle h;
  ::gnmi::Path path =
      GetPath("interfaces")("interface", "device1.domain.net.com:ce-1/1")(
          "state")("admin-status")();
  EXPECT_OK(
      gnmi_publisher_->SubscribePeriodic(Periodic(1000), path, &stream, &h));

  EXPECT_CALL(stream, Write(_, _)).WillOnce(Return(true));

  // Mock implementation of RetrieveValue() that sends a response set to
  // ADMIN_STATE_ENABLED.
  EXPECT_CALL(switch_mock_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArgs<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_admin_status()->set_state(
                            ADMIN_STATE_ENABLED);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  EXPECT_OK(gnmi_publisher_->HandleChange(TimerEvent()));
}

TEST_F(SubscriptionTest, OnUpdateUnSupportedPath) {
  // Configure the device - the model will reconfigure itself to reflect the
  // configuration.
  ASSERT_OK(
      gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config_)));

  ::gnmi::Path path = GetPath("blah")();
  ::gnmi::TypedValue val;

  EXPECT_THAT(gnmi_publisher_->HandleUpdate(path, val, nullptr).ToString(),
              HasSubstr("unsupported"));
}

TEST_F(SubscriptionTest, OnReplaceUnSupportedPath) {
  // Configure the device - the model will reconfigure itself to reflect the
  // configuration.
  ASSERT_OK(
      gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config_)));

  ::gnmi::Path path = GetPath("blah")();
  ::gnmi::TypedValue val;

  EXPECT_THAT(gnmi_publisher_->HandleReplace(path, val, nullptr).ToString(),
              HasSubstr("unsupported"));
}

TEST_F(SubscriptionTest, OnDeleteUnSupportedPath) {
  // Configure the device - the model will reconfigure itself to reflect the
  // configuration.
  ASSERT_OK(
      gnmi_publisher_->HandleChange(ConfigHasBeenPushedEvent(hal_config_)));

  ::gnmi::Path path = GetPath("blah")();
  ::gnmi::TypedValue val;

  EXPECT_THAT(gnmi_publisher_->HandleDelete(path, nullptr).ToString(),
              HasSubstr("unsupported"));
}

// Checks if the message sent by SendSyncResponse() is well-formed.
TEST_F(SubscriptionTest, SyncResponseMsgIsCorrect) {
  SubscribeReaderWriterMock stream;
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(DoAll(SaveArg<0>(&resp), Return(true)));

  EXPECT_OK(gnmi_publisher_->SendSyncResponse(&stream));
  EXPECT_TRUE(resp.sync_response());
}

// Checks if SendSyncResponse() responses correctly to 'stream' being nullptr.
TEST_F(SubscriptionTest, SyncResponseStreamNullptr) {
  EXPECT_THAT(
      gnmi_publisher_->SendSyncResponse(nullptr /* stream */).error_message(),
      HasSubstr("null"));
}

// Checks if SendSyncResponse() responses correctly to Write() to 'stream'
// reporting error.
TEST_F(SubscriptionTest, SyncResponseWriteError) {
  SubscribeReaderWriterMock stream;
  EXPECT_CALL(stream, Write(_, _)).WillOnce(Return(false));

  EXPECT_THAT(gnmi_publisher_->SendSyncResponse(&stream).error_message(),
              HasSubstr("failed"));
}

TEST_F(SubscriptionTest, CheckConvertTargetDefinedToOnChange) {
  ::gnmi::Subscription subscription;
  subscription.set_mode(::gnmi::SubscriptionMode::TARGET_DEFINED);

  EXPECT_OK(
      gnmi_publisher_->UpdateSubscriptionWithTargetSpecificModeSpecification(
          GetPath("interfaces")("interface", "*")(), &subscription));

  // ON_CHANGE is the default target-defined mode.
  EXPECT_EQ(subscription.mode(), ::gnmi::SubscriptionMode::ON_CHANGE);
  EXPECT_EQ(subscription.sample_interval(), 0);
  EXPECT_EQ(subscription.heartbeat_interval(), 0);
  EXPECT_EQ(subscription.suppress_redundant(), false);
}

// There is (almost) infinite number of possible YANG model paths. Not all of
// them are supported. The following test makes sure that all paths that were
// promised are really supported. The methods exposed by the GnmiPublisher
// return ::util::OkStatus only if the path/mode combination is supported.

// Some of the paths support only OnChange mode, so, they cannot be tested by
// the parametrized test below.
TEST_F(SubscriptionTest, PromisedOnChangeOnlyLeafsAreSupported) {
  SubscribeReaderWriterMock stream;
  SubscriptionHandle h;

  EXPECT_OK(gnmi_publisher_->SubscribeOnChange(
      GetPath("interfaces")("interface", "*")(), &stream, &h));
  EXPECT_OK(gnmi_publisher_->SubscribeOnChange(
      GetPath("interfaces")("interface")("...")(), &stream, &h));
}

// Some of the paths support only OnPoll mode, so, they cannot be tested by
// the parametrized test below.
TEST_F(SubscriptionTest, PromisedOnPollOnlyLeafsAreSupported) {
  SubscribeReaderWriterMock stream;
  SubscriptionHandle h;

  EXPECT_OK(gnmi_publisher_->SubscribePoll(
      GetPath("debug")("nodes")(
          "node", "xy1switch.prod.google.com")("packet-io")("debug-string")(),
      &stream, &h));
}

// All remaining paths support all modes and can be tested by this parametrized
// test that takes the path as a parameter.
class SubscriptionSupportedPathsTest
    : public SubscriptionTestBase,
      public ::testing::TestWithParam<::gnmi::Path> {};

TEST_P(SubscriptionSupportedPathsTest, PromisedLeafsAreSupported) {
  SubscribeReaderWriterMock stream;
  SubscriptionHandle h;

  ::gnmi::Path path = GetParam();

  EXPECT_OK(gnmi_publisher_->SubscribeOnChange(path, &stream, &h));
  EXPECT_OK(
      gnmi_publisher_->SubscribePeriodic(Periodic(1000), path, &stream, &h));
  EXPECT_OK(gnmi_publisher_->SubscribePoll(path, &stream, &h));
}

// TODO: add all supported paths here!
INSTANTIATE_TEST_CASE_P(
    SubscriptionSupportedOtherPathsTestWithPath, SubscriptionSupportedPathsTest,
    ::testing::Values(
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "oper-status")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "admin-status")(),
        GetPath("interfaces")("interface",
                              "ju1u1t1.xyz99.net.google.com:ce-1/1")("state")(
            "health-indicator")(),
        GetPath("interfaces")("interface",
                              "ju1u1t1.xyz99.net.google.com:ce-1/1")("config")(
            "health-indicator")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")(
            "ethernet")("config")("port-speed")(),
        GetPath("lacp")("interfaces")("interface",
                                      "device1.domain.net.com:ce-1/1")(
            "state")("system-id-mac")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")(
            "ethernet")("state")("port-speed")(),
        GetPath("lacp")("interfaces")("interface",
                                      "device1.domain.net.com:ce-1/1")(
            "state")("system-priority")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")(
            "ethernet")("config")("mac-address")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")(
            "ethernet")("state")("mac-address")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")(
            "ethernet")("state")("forwarding-viable")(),
        GetPath("interfaces")("interface",
                              "ju1u1t1.xyz99.net.google.com:ce-1/1")(
            "ethernet")("config")("forwarding-viable")(),
        GetPath("interfaces")("interface",
                              "ju1u1t1.xyz99.net.google.com:ce-1/1")(
            "ethernet")("state")("negotiated-port-speed")()));

// Due to Google's restriction on the size of a function frame, this automation
// had to be split into separate calls.
INSTANTIATE_TEST_CASE_P(
    SubscriptionSupportedAlarmPathsTestWithPath, SubscriptionSupportedPathsTest,
    ::testing::Values(
        GetPath("components")("component", "device1.domain.net.com")(
            "chassis")("alarms")("memory-error")("status")(),
        GetPath("components")("component", "device1.domain.net.com")(
            "chassis")("alarms")("memory-error")("time-created")(),
        GetPath("components")("component", "device1.domain.net.com")(
            "chassis")("alarms")("memory-error")("info")(),
        GetPath("components")("component", "device1.domain.net.com")(
            "chassis")("alarms")("memory-error")("severity")(),
        GetPath("components")("component", "device1.domain.net.com")(
            "chassis")("alarms")("memory-error")(),
        GetPath("components")("component", "device1.domain.net.com")(
            "chassis")("alarms")("flow-programming-exception")("status")(),
        GetPath("components")("component",
                              "device1.domain.net.com")("chassis")(
            "alarms")("flow-programming-exception")("time-created")(),
        GetPath("components")("component", "device1.domain.net.com")(
            "chassis")("alarms")("flow-programming-exception")("info")(),
        GetPath("components")("component", "device1.domain.net.com")(
            "chassis")("alarms")("flow-programming-exception")("severity")(),
        GetPath("components")("component", "device1.domain.net.com")(
            "chassis")("alarms")("flow-programming-exception")()));

// Due to Google's restriction on the size of a function frame, this automation
// had to be split into separate calls.
INSTANTIATE_TEST_CASE_P(
    SubscriptionSupportedCounterPathsTestWithPath,
    SubscriptionSupportedPathsTest,
    ::testing::Values(
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("in-octets")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("out-octets")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("in-unicast-pkts")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("out-unicast-pkts")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("in-discards")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("out-discards")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("in-unknown-protos")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("in-errors")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("out-errors")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("in-fcs-errors")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("in-broadcast-pkts")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("out-broadcast-pkts")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("in-multicast-pkts")(),
        GetPath("interfaces")("interface",
                              "device1.domain.net.com:ce-1/1")("state")(
            "counters")("out-multicast-pkts")()));

// Due to Google's restriction on the size of a function frame, this automation
// had to be split into separate calls.
INSTANTIATE_TEST_CASE_P(
    SubscriptionSupportedQosCounterPathsTestWithPath,
    SubscriptionSupportedPathsTest,
    ::testing::Values(
        GetPath("qos")("interfaces")("interface",
                                     "device1.domain.net.com:ce-1/1")(
            "output")("queues")("queue", "BE1")("state")("name")(),
        GetPath("qos")("interfaces")("interface",
                                     "device1.domain.net.com:ce-1/1")(
            "output")("queues")("queue", "BE1")("state")("id")(),
        GetPath("qos")("interfaces")("interface",
                                     "device1.domain.net.com:ce-1/1")(
            "output")("queues")("queue", "BE1")("state")("transmit-pkts")(),
        GetPath("qos")("interfaces")("interface",
                                     "device1.domain.net.com:ce-1/1")(
            "output")("queues")("queue", "BE1")("state")("transmit-octets")(),
        GetPath("qos")("interfaces")("interface",
                                     "device1.domain.net.com:ce-1/1")(
            "output")("queues")("queue", "BE1")("state")("dropped-pkts")()));

// All paths that support OnReplace only be tested by this parametrized
// test that takes the path as a parameter.
class ReplaceSupportedPathsTest
    : public SubscriptionTestBase,
      public ::testing::TestWithParam<::gnmi::Path> {};

TEST_P(ReplaceSupportedPathsTest, PromisedLeafsAreSupported) {
  SubscribeReaderWriterMock stream;
  SubscriptionHandle h;

  ::gnmi::Path path = GetParam();
  ::gnmi::TypedValue val;
  CopyOnWriteChassisConfig config(&hal_config_);

  auto status = gnmi_publisher_->HandleReplace(path, val, &config);
  if (!status.ok()) {
    EXPECT_THAT(status.ToString(), Not(HasSubstr("unsupported")));
  }
}

// Due to Google's restriction on the size of a function frame, this automation
// had to be split into separate calls.
INSTANTIATE_TEST_CASE_P(ReplaceSupportedPathsTestWithPath,
                        ReplaceSupportedPathsTest,
                        ::testing::Values(GetPath()()));

}  // namespace hal
}  // namespace stratum
