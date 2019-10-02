// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

#include "stratum/hal/lib/common/yang_parse_tree_mock.h"

#include "google/protobuf/text_format.h"
#include "gnmi/gnmi.pb.h"
#include "openconfig/openconfig.pb.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/common/subscribe_reader_writer_mock.h"
#include "stratum/hal/lib/common/switch_mock.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/lib/utils.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/test_utils/matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {

using ::testing::_;
using ::testing::ContainsRegex;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::WithArg;
using ::testing::WithArgs;
using ::stratum::test_utils::StatusIs;

class YangParseTreeTest : public ::testing::Test {
 protected:
  using OnEventAction = GnmiEventHandler (TreeNode::*)() const;
  using OnSetAction = GnmiSetHandler (TreeNode::*)() const;

  static constexpr int kInterface1NodeId = 3;
  static constexpr int kInterface1PortId = 3;
  static constexpr uint32 kInterface1QueueId = 0;
  static constexpr char kInterface1QueueName[] = "BE1";
  static constexpr char kAlarmDescription[] = "alarm";
  static constexpr char kAlarmSeverityText[] = "CRITICAL";
  static constexpr Alarm::Severity kAlarmSeverityEnum = Alarm::CRITICAL;
  static constexpr uint64 kAlarmTimeCreated = 12345ull;
  static constexpr bool kAlarmStatusTrue = true;
  static constexpr uint64 kInterfaceMac = 0x112233445566ull;

  YangParseTreeTest() : parse_tree_(&switch_) {}

  void SetUp() override {}

  void PrintNode(const TreeNode& node, const std::string& prefix) const {
    LOG(INFO) << prefix << node.name();
    for (const auto& entry : node.children_) {
      PrintNode(entry.second, prefix + " ");
    }
  }

  void PrintNodeWithOnTimer(const TreeNode& node,
                            const std::string& prefix) const {
    LOG(INFO) << prefix << node.name() << ": "
              << node.AllSubtreeLeavesSupportOnTimer() << " "
              << node.supports_on_timer_;
    for (const auto& entry : node.children_) {
      PrintNodeWithOnTimer(entry.second, prefix + " ");
    }
  }

  void PrintPath(const ::gnmi::Path& path) const {
    LOG(INFO) << path.ShortDebugString();
  }

  const TreeNode& GetRoot() const { return parse_tree_.root_; }

  TreeNode* AddNode(const ::gnmi::Path& path) {
    absl::WriterMutexLock l(&parse_tree_.root_access_lock_);
    return parse_tree_.AddNode(path);
  }

  // A proxy for YangParseTree::PerformActionForAllNonWildcardNodes().
  ::util::Status PerformActionForAllNonWildcardNodes(
      const gnmi::Path& path, const gnmi::Path& subpath,
      const std::function<::util::Status(const TreeNode& leaf)>& action) const {
    absl::WriterMutexLock l(&parse_tree_.root_access_lock_);

    return parse_tree_.PerformActionForAllNonWildcardNodes(path, subpath,
                                                           action);
  }

  // A proxy for YangParseTree::gnmi_event_writer_.
  void SetGnmiEventWriter(WriterInterface<GnmiEventPtr>* channel) {
    absl::WriterMutexLock l(&parse_tree_.root_access_lock_);

    parse_tree_.gnmi_event_writer_.reset(channel);
  }

  // A proxy for YangParseTree::AddSubtreeInterface().
  void AddSubtreeInterface(const std::string& name) {
    absl::WriterMutexLock l(&parse_tree_.root_access_lock_);

    // Add one singleton port.
    SingletonPort singleton;
    singleton.set_name(name);
    singleton.set_node(kInterface1NodeId);
    singleton.set_id(kInterface1PortId);
    singleton.set_speed_bps(kTwentyFiveGigBps);
    singleton.mutable_config_params()->set_mac_address(kInterfaceMac);
    // Add one per port per queue stat for this interface.
    NodeConfigParams node_config;
    {
      auto* entry =
          node_config.mutable_qos_config()->add_traffic_class_mapping();
      entry->set_internal_priority(2);  // some internal priority
      entry->set_traffic_class(TrafficClass::BE1);
    }
    {
      auto* entry = node_config.mutable_qos_config()->add_cosq_mapping();
      entry->set_internal_priority(2);  // some internal priority
      entry->set_q_num(kInterface1QueueId);
    }
    parse_tree_.AddSubtreeInterfaceFromSingleton(singleton, node_config);
  }

  // A proxy for YangParseTree::AddSubtreeChassis().
  void AddSubtreeChassis(const std::string& name) {
    absl::WriterMutexLock l(&parse_tree_.root_access_lock_);

    Chassis chassis;
    chassis.set_name(name);
    parse_tree_.AddSubtreeChassis(chassis);
  }

  // A proxy for YangParseTree::AddSubtreeNode().
  void AddSubtreeNode(const std::string& name, uint64 node_id) {
    absl::WriterMutexLock l(&parse_tree_.root_access_lock_);

    Node node;
    node.set_name(name);
    node.set_id(node_id);
    parse_tree_.AddSubtreeNode(node);
  }

  // A method helping testing if the OnXxx method of a leaf specified by 'path'.
  // It takes care of all the boiler plate code:
  // - adds an interface named "interface-1"
  // - adds a node named "node-1"
  // - creates a stream that will write the response proto-buf into 'resp'
  // - finds the node in the parse three
  // - gets the requested handler
  // - calls the handler with 'event'
  // - returns status produced by execution of the handler.
  ::util::Status ExecuteOnAction(const ::gnmi::Path& path,
                                 const OnEventAction& action,
                                 const GnmiEvent& event,
                                 ::gnmi::SubscribeResponse* resp) {
    // After tree creation only two leafs are defined:
    // /interfaces/interface[name=*]/state/ifindex
    // /interfaces/interface[name=*]/state/name

    // The test requires one interface branch to be added.
    AddSubtreeInterface("interface-1");
    // The test requires one node branch to be added.
    AddSubtreeNode("node-1", kInterface1NodeId);

    // Mock gRPC stream that copies parameter of Write() to 'resp'. The contents
    // of the 'resp' variable is then checked.
    SubscribeReaderWriterMock stream;
    EXPECT_CALL(stream, Write(_, _))
        .WillOnce(DoAll(
            WithArgs<0>(Invoke(
                [resp](const ::gnmi::SubscribeResponse& r) { *resp = r; })),
            Return(true)));

    // Find the leaf under test.
    auto* node = GetRoot().FindNodeOrNull(path);
    if (node == nullptr) {
      return MAKE_ERROR() << "Cannot find the requested path.";
    }

    // Get its 'action' handler and call it.
    const auto& handler = (node->*action)();
    return handler(event, &stream);
  }

  // A method helping testing if the OnPoll method of a leaf specified by
  // 'path'. It calls ExecuteOnAction() that takes care of all the boiler plate
  // code:
  // - adds an interface named "interface-1"
  // - creates a stream that will write the response proto-buf into 'resp'
  // - finds the node in the parse three
  // - gets the OnPoll event handler
  // - calls the handler with PollEvent event
  // - returns status produced by execution of the handler.
  // The caller can then check if the contents of 'resp' is the expected one
  // (assuming the returned status is ::util::OkStatus())
  ::util::Status ExecuteOnPoll(const ::gnmi::Path& path,
                               ::gnmi::SubscribeResponse* resp) {
    return ExecuteOnAction(path, &TreeNode::GetOnPollHandler, PollEvent(),
                           resp);
  }

  // A method helping testing if the OnChange method of a leaf specified by
  // 'path'. It calls ExecuteOnAction() that takes care of all the boiler plate
  // code:
  // - adds an interface named "interface-1"
  // - creates a stream that will write the response proto-buf into 'resp'
  // - finds the node in the parse three
  // - gets the OnChange event handler
  // - calls the handler with 'event' event
  // - returns status produced by execution of the handler.
  // The caller can then check if the contents of 'resp' is the expected one
  // (assuming the returned status is ::util::OkStatus())
  ::util::Status ExecuteOnChange(const ::gnmi::Path& path,
                                 const GnmiEvent& event,
                                 ::gnmi::SubscribeResponse* resp) {
    return ExecuteOnAction(path, &TreeNode::GetOnChangeHandler, event, resp);
  }

  // A method helping testing if the OnChange method of
  // /components/component/chassis/alarms sub-tree leaf specified by 'path'.
  // It takes care of all the boiler plate code:
  // - adds a chassis named "chassis-1"
  // - creates a stream that will write the response proto-buf into 'resp'
  // - finds the node in the parse three
  // - gets the OnChange event handler
  // - calls the handler with event of type 'A'
  // - returns status produced by execution of the handler.
  // - checks if the received response in field that is read using 'get_value'
  //   of type 'U' is equal to 'expected_value' of type 'V'
  template <class A, class U, class V>
  void TestOnChangeAlarmLeaf(const ::gnmi::Path& path,
                             U (::gnmi::TypedValue::*get_value)() const,
                             V expected_value) {
    // The test requires chassis component branch to be added.
    AddSubtreeChassis("chassis-1");

    // Call the event handler. 'resp' will contain the message that is sent to
    // the controller.
    ::gnmi::SubscribeResponse resp;
    ASSERT_OK(
        ExecuteOnChange(path, A(kAlarmTimeCreated, kAlarmDescription), &resp));

    // Check that the result of the call is what is expected.
    ASSERT_THAT(resp.update().update(), SizeIs(1));
    EXPECT_EQ((resp.update().update(0).val().*get_value)(), expected_value);
  }

  // A method helping testing if the OnPoll method of
  // /components/component/chassis/alarms sub-tree leaf specified by 'path'.
  // It takes care of all the boiler plate code:
  // - adds a chassis named "chassis-1"
  // - creates a stream that will write the response proto-buf into 'resp' with
  //   value 'conf_value' of type 'W'
  // - finds the node in the parse three
  // - gets the OnPoll event handler
  // - calls the handler with PollEvent event
  // - returns status produced by execution of the handler.
  // - checks if the received response in field that is read using 'get_value'
  //   of type 'U' is equal to 'expected_value' of type 'V'
  template <class U, class V, class W, class Y>
  void TestOnPollAlarmLeaf(const ::gnmi::Path& path,
                           U (::gnmi::TypedValue::*get_value)() const,
                           Alarm* (DataResponse::*mutable_alarm)(),
                           void (Alarm::*set_value)(Y), V expected_value,
                           W conf_value) {
    // The test requires chassis component branch to be added.
    AddSubtreeChassis("chassis-1");

    // Mock implementation of RetrieveValue() that sends a response set to
    // 'expected_value'.
    EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
        .WillOnce(DoAll(
            WithArg<2>(Invoke([mutable_alarm, set_value,
                               conf_value](WriterInterface<DataResponse>* w) {
              DataResponse resp;
              // Set the response.
              ((resp.*mutable_alarm)()->*set_value)(conf_value);
              // Send it to the caller.
              w->Write(resp);
            })),
            Return(::util::OkStatus())));

    // Call the event handler. 'resp' will contain the message that is sent to
    // the controller.
    ::gnmi::SubscribeResponse resp;
    ASSERT_OK(ExecuteOnPoll(path, &resp));

    // Check that the result of the call is what is expected.
    ASSERT_THAT(resp.update().update(), SizeIs(1));
    EXPECT_EQ((resp.update().update(0).val().*get_value)(), expected_value)
        << resp.ShortDebugString();
  }

  // A specialization of generic template method TestOnPollAlarmLeaf().
  // It is used when 'expected_value' and 'conf_value' are the same.
  template <class U, class V>
  void TestOnPollAlarmLeaf(const ::gnmi::Path& path,
                           U (::gnmi::TypedValue::*get_value)() const,
                           Alarm* (DataResponse::*mutable_alarm)(),
                           void (Alarm::*set_value)(U), V expected_value) {
    TestOnPollAlarmLeaf(path, get_value, mutable_alarm, set_value,
                        expected_value, expected_value);
  }

  // A method helping testing if the OnXxx method of a leaf specified by 'path'.
  // It takes care of all the boiler plate code:
  // - adds an interface named "interface-1"
  // - adds a node named "node-1"
  // - creates a mock method that will write the set request proto-buf into
  //   'req'
  // - finds the node in the parse three
  // - gets the requested handler
  // - calls the handler with 'val'
  // - returns status produced by execution of the handler.
  ::util::Status ExecuteOnSet(const ::gnmi::Path& path,
                              const OnSetAction& action,
                              const ::google::protobuf::Message& val,
                              SetRequest* req, GnmiEventPtr* notification) {
    // After tree creation only two leafs are defined:
    // /interfaces/interface[name=*]/state/ifindex
    // /interfaces/interface[name=*]/state/name

    // The test requires one interface branch to be added.
    AddSubtreeInterface("interface-1");
    // The test requires one node branch to be added.
    AddSubtreeNode("node-1", kInterface1NodeId);
    // Make a copy-on-write pointer to current chassis configuration.
    ChassisConfig chassis_config;
    CopyOnWriteChassisConfig config(&chassis_config);

    // Expect the SetValue() call only if the 'req' is not nullptr.
    if (req) {
      EXPECT_CALL(switch_, SetValue(_, _, _))
          .WillOnce(DoAll(
              WithArgs<1>(Invoke([req](const SetRequest& r) { *req = r; })),
              Return(::util::OkStatus())));
    } else {
      EXPECT_CALL(switch_, SetValue(_, _, _)).Times(0);
    }

    if (notification) {
      EXPECT_CALL(parse_tree_, SendNotification(_))
          .WillOnce(WithArg<0>(Invoke(
              [notification](const GnmiEventPtr& n) { *notification = n; })));
    } else {
      EXPECT_CALL(parse_tree_, SendNotification(_)).Times(0);
    }
    // Find the leaf under test.
    auto* node = GetRoot().FindNodeOrNull(path);
    if (node == nullptr) {
      return MAKE_ERROR() << "Cannot find the requested path.";
    }

    // Get its 'action' handler and call it.
    const auto& handler = (node->*action)();
    auto status = handler(path, val, &config);
    if (config.HasBeenChanged()) delete config.PassOwnership();
    return status;
  }

  // A method helping testing if the OnUpdate method of a leaf specified by
  // 'path'. It calls ExecuteOnSet() that takes care of all the boiler plate
  // code:
  // - adds an interface named "interface-1"
  // - adds a node named "node-1"
  // - creates a mock method that will write the set request proto-buf into
  //   'req'
  // - finds the node in the parse three
  // - gets the requested OnUpdate handler
  // - calls the handler with 'val'
  // - returns status produced by execution of the handler.
  // The caller can then check if the contents of 'req' is the expected one
  // (assuming the returned status is ::util::OkStatus())
  ::util::Status ExecuteOnUpdate(const ::gnmi::Path& path,
                                 const ::google::protobuf::Message& val,
                                 SetRequest* req, GnmiEventPtr* notification) {
    return ExecuteOnSet(path, &TreeNode::GetOnUpdateHandler, val, req,
                        notification);
  }

  // A method helping testing if the OnReplace method of a leaf specified by
  // 'path'. It calls ExecuteOnSet() that takes care of all the boiler plate
  // code:
  // - adds an interface named "interface-1"
  // - adds a node named "node-1"
  // - creates a mock method that will write the set request proto-buf into
  //   'req'
  // - finds the node in the parse three
  // - gets the requested OnReplace handler
  // - calls the handler with 'val'
  // - returns status produced by execution of the handler.
  // The caller can then check if the contents of 'req' is the expected one
  // (assuming the returned status is ::util::OkStatus())
  ::util::Status ExecuteOnReplace(const ::gnmi::Path& path,
                                  const ::google::protobuf::Message& val,
                                  SetRequest* req, GnmiEventPtr* notification) {
    return ExecuteOnSet(path, &TreeNode::GetOnReplaceHandler, val, req,
                        notification);
  }

  // A mock of a switch that implements the switch interface.
  SwitchMock switch_;
  // The implementation under test.
  YangParseTreeMock parse_tree_;
  // A gnmi::Path comparator.
  PathComparator compare_;
};

constexpr char YangParseTreeTest::kInterface1QueueName[];
constexpr char YangParseTreeTest::kAlarmDescription[];
constexpr char YangParseTreeTest::kAlarmSeverityText[];
constexpr uint64 YangParseTreeTest::kAlarmTimeCreated;
constexpr bool YangParseTreeTest::kAlarmStatusTrue;
constexpr uint32 YangParseTreeTest::kInterface1QueueId;

TEST_F(YangParseTreeTest, LazyOneTimeCopyOnWritePtrModifiedViaPtr) {
  ChassisConfig config;
  CopyOnWriteChassisConfig lazy_config(&config);

  // Check that the lazy copy has not been modified.
  // const std::string description = lazy_config->description();
  // EXPECT_EQ(description.size(), 0);
  EXPECT_EQ(lazy_config->nodes_size(), 0);
  EXPECT_FALSE(lazy_config.HasBeenChanged());

  // Modify the lazy copy
  lazy_config.writable()->set_description("test");

  // Check that only the lazy copy has been modified.
  EXPECT_TRUE(lazy_config.HasBeenChanged());
  EXPECT_THAT(lazy_config->description(), HasSubstr("test"));
  EXPECT_EQ(config.description().size(), 0);
}

TEST_F(YangParseTreeTest, LazyOneTimeCopyOnWritePtrModifiedViaRef) {
  ChassisConfig config;
  CopyOnWriteChassisConfig lazy_config(&config);

  // Check that the lazy copy has not been modified.
  const std::string description = (*lazy_config).description();
  EXPECT_EQ(description.size(), 0);
  EXPECT_FALSE(lazy_config.HasBeenChanged());

  // Modify the lazy copy
  (*lazy_config.writable()).set_description("test");

  // Check that only the lazy copy has been modified.
  EXPECT_TRUE(lazy_config.HasBeenChanged());
  EXPECT_THAT((*lazy_config).description(), HasSubstr("test"));
  EXPECT_EQ(config.description().size(), 0);
  delete lazy_config.PassOwnership();
}

TEST_F(YangParseTreeTest, CopySubtree) { PrintNode(GetRoot(), ""); }

TEST_F(YangParseTreeTest, AllSupportOnTime) {
  EXPECT_FALSE(GetRoot().AllSubtreeLeavesSupportOnTimer());
  PrintNodeWithOnTimer(GetRoot(), "");
}

TEST_F(YangParseTreeTest, AllSupportOnChange) {
  EXPECT_TRUE(GetRoot().AllSubtreeLeavesSupportOnChange());
}

TEST_F(YangParseTreeTest, AllSupportOnPoll) {
  EXPECT_TRUE(GetRoot().AllSubtreeLeavesSupportOnPoll());
}

TEST_F(YangParseTreeTest, AllSupportOnUpdate) {
  EXPECT_FALSE(GetRoot().AllSubtreeLeavesSupportOnUpdate());
}

TEST_F(YangParseTreeTest, AllSupportOnReplace) {
  EXPECT_FALSE(GetRoot().AllSubtreeLeavesSupportOnReplace());
}

TEST_F(YangParseTreeTest, AllSupportOnDelete) {
  EXPECT_FALSE(GetRoot().AllSubtreeLeavesSupportOnDelete());
}

TEST_F(YangParseTreeTest, GetPathWithoutKey) {
  auto path =
      GetRoot().FindNodeOrNull(GetPath("interfaces")("interface")())->GetPath();
  PrintPath(path);
  ASSERT_EQ(path.elem_size(), 2);
  EXPECT_EQ(path.elem(0).name(), "interfaces");
  EXPECT_EQ(path.elem(0).key_size(), 0);
  EXPECT_EQ(path.elem(1).name(), "interface");
  EXPECT_EQ(path.elem(1).key_size(), 0);
}

TEST_F(YangParseTreeTest, GetPathWithKey) {
  auto path = GetRoot()
                  .FindNodeOrNull(GetPath("interfaces")("interface", "*")())
                  ->GetPath();
  PrintPath(path);
  ASSERT_EQ(path.elem_size(), 2);
  EXPECT_EQ(path.elem(0).name(), "interfaces");
  EXPECT_EQ(path.elem(0).key_size(), 0);
  EXPECT_EQ(path.elem(1).name(), "interface");
  ASSERT_EQ(path.elem(1).key_size(), 1);
  EXPECT_EQ(path.elem(1).key().at("name"), "*");
}

TEST_F(YangParseTreeTest, FindRoot) {
  auto path = GetPath()();
  ASSERT_EQ(path.elem_size(), 0);
  auto node = GetRoot().FindNodeOrNull(path);
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node, &GetRoot());
  auto root = AddNode(GetPath()());
  ASSERT_EQ(node, root);

  openconfig::Device device;
  ASSERT_OK(ReadProtoFromTextFile(
      "stratum/hal/lib/common/testdata/simple_oc_device.pb.txt", &device));

  string msg_bytes;
  device.SerializeToString(&msg_bytes);

  // Prepare a SET request.
  ::gnmi::TypedValue req;
  req.set_bytes_val(msg_bytes);

  ChassisConfig config;
  CopyOnWriteChassisConfig copy_on_write_config(&config);
  ASSERT_OK(node->GetOnReplaceHandler()(path, req, &copy_on_write_config));
}

TEST_F(YangParseTreeTest, PerformActionForAllNodesNonePresent) {
  // After tree creation only two leafs are defined:
  // /interfaces/interface[name=*]/state/ifindex
  // /interfaces/interface[name=*]/state/name

  int counter = 0;

  const auto& action = [&counter](const TreeNode& leaf) {
    // Count every execution of this action.
    ++counter;
    return ::util::OkStatus();
  };

  EXPECT_OK(PerformActionForAllNonWildcardNodes(
      GetPath("interfaces")("interface")(), GetPath("state")("ifindex")(),
      action));

  // The action should never be called as there is no nodes in the tree matching
  // the request.
  EXPECT_EQ(0, counter);
}

// Check if the action is executed for all qualified leafs.
TEST_F(YangParseTreeTest, PerformActionForAllNodesOnePresent) {
  // After tree creation only two leafs are defined:
  // /interfaces/interface[name=*]/state/ifindex
  // /interfaces/interface[name=*]/state/name

  // The test requires one interface branch to be added.
  AddSubtreeInterface("interface-1");

  std::vector<const TreeNode*> nodes;

  const auto& action = [&nodes](const TreeNode& leaf) {
    // Store every leaf this action was executed on.
    nodes.push_back(&leaf);
    return ::util::OkStatus();
  };

  EXPECT_OK(PerformActionForAllNonWildcardNodes(
      GetPath("interfaces")("interface")(), GetPath("state")("ifindex")(),
      action));

  // The action should be called once as there is one node in the tree matching
  // the request.
  ASSERT_EQ(1, nodes.size());
  EXPECT_FALSE(compare_(
      nodes.at(0)->GetPath(),
      GetPath("interfaces")("interface", "interface-1")("state")("ifindex")()));
}

// Check if RetrieveValue is called.
TEST_F(YangParseTreeTest, GetDataFromSwitchInterfaceCalled) {
  // Create a fake switch interface object.
  SwitchMock switch_interface;
  EXPECT_CALL(switch_interface, RetrieveValue(_, _, _, _))
      .WillOnce(Return(::util::OkStatus()));

  // Create a data retrieval request.
  uint64 node_id = 0;
  DataRequest req;
  DataResponseWriter writer([](const DataResponse&) { return true; });
  // Request the data.
  EXPECT_OK(switch_interface.RetrieveValue(node_id, req, &writer, nullptr));
}

// Check if the response message is set correctly.
TEST_F(YangParseTreeTest, GetDataFromSwitchInterfaceDataCopied) {
  // Create a fake switch interface object.
  SwitchMock switch_interface;
  EXPECT_CALL(switch_interface, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArgs<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_oper_status()->set_state(PORT_STATE_UP);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Create a data retrieval request.
  uint64 node_id = 0;
  DataRequest req;
  DataResponse resp;
  DataResponseWriter writer([&resp](const DataResponse& in) {
    resp = in;
    return true;
  });
  // Pre-test check.
  ASSERT_FALSE(resp.has_oper_status());
  // Request the data.
  EXPECT_OK(switch_interface.RetrieveValue(node_id, req, &writer, nullptr));
  // Check that the data has been modified.
  ASSERT_TRUE(resp.has_oper_status());
  EXPECT_EQ(resp.oper_status().state(), PORT_STATE_UP);
}

// A class that implements a channel that is used by the YANG tree node handlers
// to send notifications to GnmiPublisher.
class GnmiEventWriterMock : public WriterInterface<GnmiEventPtr> {
 public:
  GnmiEventWriterMock() {}

  // The only work method defined by the interface - it is called every time
  // there is a data to be processed.
  MOCK_METHOD1(Write, bool(const GnmiEventPtr& resp));
};

// Check if the notification message is send correctly.
TEST_F(YangParseTreeTest, SendNotificationPass) {
  // Always forward SendNotifications to YangParseTree::SendNotification().
  EXPECT_CALL(parse_tree_, SendNotification(_))
      .WillRepeatedly(WithArg<0>(Invoke([this](const GnmiEventPtr& n) {
        this->parse_tree_.YangParseTree::SendNotification(n);
      })));

  // Test SendNotification() without seting up the channel
  SetGnmiEventWriter(nullptr);
  parse_tree_.SendNotification(GnmiEventPtr(new PortHealthIndicatorChangedEvent(
      /*node_id*/ 0, /*port_id*/ 0, HealthState::HEALTH_STATE_BAD)));

  // Test SendNotification() with channel set up.
  // Scenario #1: correct processing.
  auto* channel = new GnmiEventWriterMock();
  SetGnmiEventWriter(channel);
  GnmiEventPtr notification;
  EXPECT_CALL(*channel, Write(_))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([&notification](const GnmiEventPtr& n) {
                  notification = n;
                })),
                Return(true)));

  EXPECT_CALL(parse_tree_, SendNotification(_))
      .WillRepeatedly(WithArg<0>(Invoke([this](const GnmiEventPtr& n) {
        this->parse_tree_.YangParseTree::SendNotification(n);
      })));

  parse_tree_.SendNotification(GnmiEventPtr(new PortHealthIndicatorChangedEvent(
      /*node_id*/ 0, /*port_id*/ 0, HealthState::HEALTH_STATE_BAD)));

  // Test SendNotification() with channel set up.
  // Scenario #3: incorrect processing.
  EXPECT_CALL(*channel, Write(_))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([&notification](const GnmiEventPtr& n) {
                  notification = n;
                })),
                Return(false)));

  parse_tree_.SendNotification(GnmiEventPtr(new PortHealthIndicatorChangedEvent(
      /*node_id*/ 0, /*port_id*/ 0, HealthState::HEALTH_STATE_BAD)));
}

// Check if the action is executed for all qualified leafs.
TEST_F(YangParseTreeTest, GetDataFromSwitchInterfaceDataConvertedCorrectly) {
  // After tree creation only two leafs are defined:
  // /interfaces/interface[name=*]/state/ifindex
  // /interfaces/interface[name=*]/state/name

  // The test requires one interface branch to be added.
  AddSubtreeInterface("interface-1");

  // Mock implementation of RetrieveValue() that sends a response set to
  // HW_STATE_READY.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArgs<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_oper_status()->set_state(PORT_STATE_UP);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Mock gRPC stream that copies parameter of Write() to 'resp'. The contents
  // of the 'resp' variable is then checked.
  SubscribeReaderWriterMock stream;
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke(
                    [&resp](const ::gnmi::SubscribeResponse& r) { resp = r; })),
                Return(true)));

  // Find the 'oper-state' leaf.
  auto* node = GetRoot().FindNodeOrNull(GetPath("interfaces")(
      "interface", "interface-1")("state")("oper-status")());
  ASSERT_NE(node, nullptr);

  // Get its OnTimer() handler and call it.
  const auto& handler = node->GetOnTimerHandler();
  EXPECT_OK(handler(TimerEvent(), &stream));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), "UP");
}

// Check if the default action applying target defined mode to a
// subscription does not set the SAMPLE mode. This is needed for the following
// test to work correctly.
TEST_F(YangParseTreeTest, DefaultTargetDefinedModeIsNotSample) {
  TreeNode node;

  ::gnmi::Subscription subscription;
  ASSERT_OK(node.ApplyTargetDefinedModeToSubscription(&subscription));
  EXPECT_NE(subscription.mode(), ::gnmi::SubscriptionMode::SAMPLE);
}

// Check if changing target defined mode works correctly.
TEST_F(YangParseTreeTest, ChangeDefaultTargetDefinedMode) {
  TreeNode node;

  auto new_target_defined_mode = [](::gnmi::Subscription* subscription) {
    subscription->set_mode(::gnmi::SubscriptionMode::SAMPLE);
    return ::util::OkStatus();
  };
  ASSERT_EQ(node.SetTargetDefinedMode(new_target_defined_mode), &node);

  ::gnmi::Subscription subscription;
  ASSERT_OK(node.ApplyTargetDefinedModeToSubscription(&subscription));
  EXPECT_EQ(subscription.mode(), ::gnmi::SubscriptionMode::SAMPLE);
}

// Check if the default action applying target defined mode to a
// subscription for "/interfaces/interface/state/counters" sets it to ON_CHANGE.
TEST_F(YangParseTreeTest, DefaultTargetDefinedModeIsSampleForCounters) {
  // The test requires one interface branch to be added.
  AddSubtreeInterface("interface-1");

  const TreeNode* node = GetRoot().FindNodeOrNull(
      GetPath("interfaces")("interface", "interface-1")("state")("counters")());
  ASSERT_NE(node, nullptr);

  ::gnmi::Subscription subscription;
  ASSERT_OK(node->ApplyTargetDefinedModeToSubscription(&subscription));
  EXPECT_EQ(subscription.mode(), ::gnmi::SubscriptionMode::SAMPLE);
  EXPECT_EQ(subscription.sample_interval(), 1000);
}

// Check if the 'oper-status' OnPoll action works correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceStateOperStatusOnPollSuccess) {
  auto path = GetPath("interfaces")("interface",
                                    "interface-1")("state")("oper-status")();

  // Mock implementation of RetrieveValue() that sends a response set to
  // OPER_STATE_UP.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArgs<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_oper_status()->set_state(PORT_STATE_UP);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), "UP");
}

// Check if the 'oper-status' OnChange action works correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceStateOperStatusOnChangeSuccess) {
  auto path = GetPath("interfaces")("interface",
                                    "interface-1")("state")("oper-status")();

  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(
      ExecuteOnChange(path,
                      PortOperStateChangedEvent(
                          kInterface1NodeId, kInterface1PortId, PORT_STATE_UP),
                      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), "UP");
}

// Check if the 'admin-status' OnPoll action works correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceStateAdminStatusOnPollSuccess) {
  auto path = GetPath("interfaces")("interface",
                                    "interface-1")("state")("admin-status")();

  // Mock implementation of RetrieveValue() that sends a response set to
  // ADMIN_STATE_ENABLED.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArgs<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_admin_status()->set_state(
                            ADMIN_STATE_ENABLED);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), "UP");
}

// Check if the 'admin-status' OnChange action works correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceStateAdminStatusOnChangeSuccess) {
  auto path = GetPath("interfaces")("interface",
                                    "interface-1")("state")("admin-status")();

  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnChange(
      path,
      PortAdminStateChangedEvent(kInterface1NodeId, kInterface1PortId,
                                 ADMIN_STATE_ENABLED),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), "UP");
}

// Check if the action is executed correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceStateNameOnPollSuccess) {
  // After tree creation only two leafs are defined:
  // /interfaces/interface[name=*]/state/ifindex
  // /interfaces/interface[name=*]/state/name

  // The test requires one interface branch to be added.
  AddSubtreeInterface("interface-1");

  // Mock gRPC stream that copies parameter of Write() to 'resp'. The contents
  // of the 'resp' variable is then checked.
  SubscribeReaderWriterMock stream;
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke(
                    [&resp](const ::gnmi::SubscribeResponse& r) { resp = r; })),
                Return(true)));

  // Find the 'name' leaf.
  auto* node = GetRoot().FindNodeOrNull(
      GetPath("interfaces")("interface", "interface-1")("state")("name")());
  ASSERT_NE(node, nullptr);

  // Get its OnPoll() handler and call it.
  const auto& handler = node->GetOnPollHandler();
  EXPECT_OK(handler(PollEvent(), &stream));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), "interface-1");
}

// Check if the action is executed correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceStateIfIndexOnPollSuccess) {
  // After tree creation only two leafs are defined:
  // /interfaces/interface[name=*]/state/ifindex
  // /interfaces/interface[name=*]/state/name

  // The test requires one interface branch to be added.
  AddSubtreeInterface("interface-1");

  // Mock gRPC stream that copies parameter of Write() to 'resp'. The contents
  // of the 'resp' variable is then checked.
  SubscribeReaderWriterMock stream;
  ::gnmi::SubscribeResponse resp;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke(
                    [&resp](const ::gnmi::SubscribeResponse& r) { resp = r; })),
                Return(true)));

  // Find the 'ifindex' leaf.
  auto* node = GetRoot().FindNodeOrNull(
      GetPath("interfaces")("interface", "interface-1")("state")("ifindex")());
  ASSERT_NE(node, nullptr);

  // Get its OnPoll() handler and call it.
  const auto& handler = node->GetOnPollHandler();
  EXPECT_OK(handler(PollEvent(), &stream));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), 3);
}

// Check if the 'state/mac-address' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceEthernetStateMacAddressOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("state")("mac-address")();
  static constexpr char kMacAddressAsString[] = "11:22:33:44:55:66";
  static constexpr uint64 kMacAddress = 0x112233445566ull;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kMacAddress.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_mac_address()->set_mac_address(
                            kMacAddress);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(), kMacAddressAsString);
}

// Check if the 'state/mac-address' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceEthernetStateMacAddressOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("state")("mac-address")();
  static constexpr char kMacAddressAsString[] = "11:22:33:44:55:66";
  static constexpr uint64 kMacAddress = 0x112233445566ull;

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(
      ExecuteOnChange(path,
                      PortMacAddressChangedEvent(
                          kInterface1NodeId, kInterface1PortId, kMacAddress),
                      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(), kMacAddressAsString);
}

// Check if the 'config/mac-address' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceEthernetConfigMacAddressOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("config")("mac-address")();
  static constexpr char kMacAddressAsString[] = "11:22:33:44:55:66";

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(), kMacAddressAsString);
}

// Checks if the 'state/port-speed' OnPoll action works correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceStatePortSpeedOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("state")("port-speed")();

  // Mock implementation of RetrieveValue() that sends a response set to
  // 25 GigBps.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArgs<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_speed()->set_speed_bps(
                            kTwentyFiveGigBps);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), "SPEED_25GB");
}

// Check if the 'system-priority' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       LacpInterfacesInterfaceStateSystemPriorityOnPollSuccess) {
  auto path = GetPath("lacp")("interfaces")(
      "interface", "interface-1")("state")("system-priority")();
  constexpr uint64 kLacpSystemPriority = 5;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kLacpSystemPriority.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_lacp_system_priority()->set_priority(
                            kLacpSystemPriority);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kLacpSystemPriority);
}

// Check if the 'system-priority' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       LacpInterfacesInterfaceStateSystemPriorityOnChangeSuccess) {
  auto path = GetPath("lacp")("interfaces")(
      "interface", "interface-1")("state")("system-priority")();
  constexpr uint64 kLacpSystemPriority = 5;

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortLacpSystemPriorityChangedEvent(kInterface1NodeId, kInterface1PortId,
                                         kLacpSystemPriority),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kLacpSystemPriority);
}

// Checks if the 'state/port-speed' OnChange action works correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceStatePortSpeedOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("state")("port-speed")();

  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortSpeedBpsChangedEvent(kInterface1NodeId, kInterface1PortId,
                               kTwentyFiveGigBps),
      &resp));

  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), "SPEED_25GB");
}

// Checks if the 'state/negotiated-port-speed' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceEthernetStateNegotiatedPortSpeedOnPollSuccess) {
  ::gnmi::Path path = GetPath("interfaces")("interface", "interface-1")(
      "ethernet")("state")("negotiated-port-speed")();

  // Mock implementation of RetrieveValue() that sends a response set to
  // 25 GigBps.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArgs<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_negotiated_port_speed()->set_speed_bps(
                            kTwentyFiveGigBps);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(), "SPEED_25GB");
}

// Checks if the 'state/negotiated-port-speed' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceEthernetStateNegotiatedPortSpeedOnChangeSuccess) {
  ::gnmi::Path path = GetPath("interfaces")("interface", "interface-1")(
      "ethernet")("state")("negotiated-port-speed")();

  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortNegotiatedSpeedBpsChangedEvent(kInterface1NodeId, kInterface1PortId,
                                         kTwentyFiveGigBps),
      &resp));

  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(), "SPEED_25GB");
}

// Checks if the 'config/port-speed' OnPoll action works correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceConfigPortSpeedOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("config")("port-speed")();
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), "SPEED_25GB");
}

// Check if the 'counters/in-octets' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInOctetsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-octets")();
  constexpr uint64 kInOctets = 5;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kInOctets.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_in_octets(kInOctets);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInOctets);
}

// Check if the 'counters/in-octets' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInOctetsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-octets")();
  constexpr uint64 kInOctets = 5;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_in_octets(kInOctets);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInOctets);
}

// Check if the 'counters/out-octets' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutOctetsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-octets")();
  constexpr uint64 kOutOctets = 45;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kOutOctets.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_out_octets(
                            kOutOctets);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutOctets);
}

// Check if the 'counters/out-octets' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutOctetsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-octets")();
  constexpr uint64 kOutOctets = 44;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_out_octets(kOutOctets);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutOctets);
}

// Check if the 'counters/in-unicast-pkts' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInUnicastPktsOnPollSuccess) {
  ::gnmi::Path path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-unicast-pkts")();
  constexpr uint64 kInUnicastPkts = 5;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kInOctets.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_in_unicast_pkts(
                            kInUnicastPkts);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInUnicastPkts);
}

// Check if the 'counters/in-unicast-pkts' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInUnicastPktsOnChangeSuccess) {
  ::gnmi::Path path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-unicast-pkts")();
  constexpr uint64 kInUnicastPkts = 5;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_in_unicast_pkts(kInUnicastPkts);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInUnicastPkts);
}

// Check if the 'counters/in-broadcast-pkts' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInBroadcastPktsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-broadcast-pkts")();
  constexpr uint64 kInBroadcastPkts = 5;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kInOctets.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_in_broadcast_pkts(
                            kInBroadcastPkts);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInBroadcastPkts);
}

// Check if the 'counters/out-unicast-pkts' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutUnicastPktsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-unicast-pkts")();
  constexpr uint64 kOutUnicastPkts = 5;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kOutUnicastPkts.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_out_unicast_pkts(
                            kOutUnicastPkts);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutUnicastPkts);
}

// Check if the 'counters/out-unicast-pkts' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutUnicastPktsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-unicast-pkts")();
  constexpr uint64 kOutUnicastPkts = 5;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_out_unicast_pkts(kOutUnicastPkts);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutUnicastPkts);
}

// Check if the 'counters/in-broadcast-pkts' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInBroadcastPktsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-broadcast-pkts")();
  constexpr uint64 kInBroadcastPkts = 5;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_in_broadcast_pkts(kInBroadcastPkts);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInBroadcastPkts);
}

// Check if the 'counters/out-broadcast-pkts' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutBroadcastPktsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-broadcast-pkts")();
  constexpr uint64 kOutBroadcastPkts = 5;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kOutBroadcastPkts.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_out_broadcast_pkts(
                            kOutBroadcastPkts);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutBroadcastPkts);
}

// Check if the 'counters/out-broadcast-pkts' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutBroadcastPktsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-broadcast-pkts")();
  constexpr uint64 kOutBroadcastPkts = 5;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_out_broadcast_pkts(kOutBroadcastPkts);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutBroadcastPkts);
}

// Check if the 'counters/in-discards' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInDiscardsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-discards")();
  constexpr uint64 kInDiscards = 12;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kInDiscards.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_in_discards(
                            kInDiscards);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInDiscards);
}

// Check if the 'counters/in-discards' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInDiscardsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-discards")();
  constexpr uint64 kInDiscards = 11;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_in_discards(kInDiscards);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInDiscards);
}

// Check if the 'counters/out-discards' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutDiscardsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-discards")();
  constexpr uint64 kOutDiscards = 12;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kOutDiscards.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_out_discards(
                            kOutDiscards);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutDiscards);
}

// Check if the 'counters/out-discards' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutDiscardsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-discards")();
  constexpr uint64 kOutDiscards = 11;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_out_discards(kOutDiscards);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutDiscards);
}

// Check if the 'counters/in-multicast-pkts' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInMulticastPktsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-multicast-pkts")();
  constexpr uint64 kInMulticastPkts = 5;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kInMulticastPkts.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_in_multicast_pkts(
                            kInMulticastPkts);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInMulticastPkts);
}

// Check if the 'counters/in-multicast-pkts' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInMulticastPktsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-multicast-pkts")();
  constexpr uint64 kInMulticastPkts = 5;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_in_multicast_pkts(kInMulticastPkts);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInMulticastPkts);
}

// Check if the 'counters/in-unknown-protos' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInUnknownProtosOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-unknown-protos")();
  constexpr uint64 kInUnknownProtos = 18;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kInUnknownProtos.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_in_unknown_protos(
                            kInUnknownProtos);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInUnknownProtos);
}

// Check if the 'counters/in-unknown-protos' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInUnknownProtosOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-unknown-protos")();
  constexpr uint64 kInUnknownProtos = 19;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_in_unknown_protos(kInUnknownProtos);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInUnknownProtos);
}

// Check if the 'counters/in-errors' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInErrorsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-errors")();
  constexpr uint64 kInErrors = 11;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kInErrors.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_in_errors(kInErrors);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInErrors);
}

// Check if the 'counters/in-errors' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInErrorsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-errors")();
  constexpr uint64 kInErrors = 16;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_in_errors(kInErrors);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInErrors);
}

// Check if the 'counters/out-errors' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutErrorsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-errors")();
  constexpr uint64 kOutErrors = 11;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kOutErrors.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_out_errors(
                            kOutErrors);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutErrors);
}

// Check if the 'counters/out-errors' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutErrorsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-errors")();
  constexpr uint64 kOutErrors = 16;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_out_errors(kOutErrors);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutErrors);
}

// Check if the 'counters/in-fcs-errors' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInFcsErrorsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-fcs-errors")();
  constexpr uint64 kInFcsErrors = 11;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kInFcsErrors.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_in_fcs_errors(
                            kInFcsErrors);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInFcsErrors);
}

// Check if the 'counters/in-fcs-errors' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersInFcsErrorsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("in-fcs-errors")();
  constexpr uint64 kInFcsErrors = 16;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_in_fcs_errors(kInFcsErrors);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInFcsErrors);
}

// Check if the 'counters/out-multicast-pkts' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutMulticastPktsOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-multicast-pkts")();
  constexpr uint64 kOutMulticastPkts = 5;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kOutMulticastPkts.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_counters()->set_out_multicast_pkts(
                            kOutMulticastPkts);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutMulticastPkts);
}

// Check if the 'counters/out-multicast-pkts' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateCountersOutMulticastPktsOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("counters")("out-multicast-pkts")();
  constexpr uint64 kOutMulticastPkts = 5;

  // Prepare the structure that stores the counters.
  PortCounters counters;
  counters.set_out_multicast_pkts(kOutMulticastPkts);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortCountersChangedEvent(kInterface1NodeId, kInterface1PortId, counters),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kOutMulticastPkts);
}

// Check if the 'system-id-mac' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       LacpInterfacesInterfaceStateSystemIdMacOnPollSuccess) {
  auto path = GetPath("lacp")("interfaces")(
      "interface", "interface-1")("state")("system-id-mac")();
  static constexpr char kSystemIdMacAsString[] = "11:22:33:44:55:66";
  static constexpr uint64 kSystemIdMac = 0x112233445566ull;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kOutMulticastPkts.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_lacp_router_mac()->set_mac_address(
                            kSystemIdMac);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(), kSystemIdMacAsString);
}

// Check if the 'system-id-mac' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       LacpInterfacesInterfaceStateSystemIdMacOnChangeSuccess) {
  auto path = GetPath("lacp")("interfaces")(
      "interface", "interface-1")("state")("system-id-mac")();
  static constexpr char kSystemIdMacAsString[] = "66:55:44:33:22:11";
  static constexpr uint64 kSystemIdMac = 0x665544332211ull;

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(
      ExecuteOnChange(path,
                      PortLacpRouterMacChangedEvent(
                          kInterface1NodeId, kInterface1PortId, kSystemIdMac),
                      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(), kSystemIdMacAsString);
}

// Check if the '/interfaces/interface/ethernet/state/forwarding-viable' OnPoll
// action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceEthernetStateForwardingViabilityOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("state")("forwarding-viable")();
  static constexpr bool kForwardingViableTrue = true;
  static constexpr TrunkMemberBlockState kTrunkMemberBlockStateForwarding =
      TRUNK_MEMBER_BLOCK_STATE_FORWARDING;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kForwardingViableTrue.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_forwarding_viability()->set_state(
                            kTrunkMemberBlockStateForwarding);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().bool_val(), kForwardingViableTrue);
}

// Check if the '/interfaces/interface/ethernet/state/forwarding-viable'
// OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceEthernetStateForwardingViabilityOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("state")("forwarding-viable")();
  static constexpr bool kForwardingViableTrue = true;
  static constexpr TrunkMemberBlockState kTrunkMemberBlockStateForwarding =
      TRUNK_MEMBER_BLOCK_STATE_FORWARDING;

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortForwardingViabilityChangedEvent(kInterface1NodeId, kInterface1PortId,
                                          kTrunkMemberBlockStateForwarding),
      &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().bool_val(), kForwardingViableTrue);
}

// Check if the '/interfaces/interface/ethernet/config/forwarding-viable' OnPoll
// action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceEthernetConfigForwardingViabilityOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("config")("forwarding-viable")();
  static constexpr bool kTrunkMemberBlockStateForwarding = true;
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().bool_val(),
            kTrunkMemberBlockStateForwarding);
}

// Check if the '/interfaces/interface/state/last-change' OnPoll
// action works correctly.
TEST_F(YangParseTreeTest, InterfacesInterfaceStateLastChangeOnPollSuccess) {
  auto path = GetPath("interfaces")("interface",
                                    "interface-1")("state")("last-change")();
  static constexpr char kUnsupportedString[] = "unsupported yet";
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), kUnsupportedString);
}

// Check if the '/interfaces/interface/ethernet/config/forwarding-viable'
// OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceEthernetConfigForwardingViabilityOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("ethernet")("config")("forwarding-viable")();
  // TODO(unknown): add test when gNMI SET operation is implemented.
}

// Check if the '/interfaces/interface/state/health-indicator' OnPoll
// action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateHealthIndicatorOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("health-indicator")();
  static constexpr auto kHealthIndicatorGood = HEALTH_STATE_GOOD;
  static constexpr char kHealthIndicatorGoodString[] = "GOOD";

  // Mock implementation of RetrieveValue() that sends a response set to
  // kHealthIndicatorGood.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_health_indicator()->set_state(
                            kHealthIndicatorGood);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(),
            kHealthIndicatorGoodString);
}

// Check if the '/interfaces/interface/state/health-indicator'
// OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceStateHealthIndicatorOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("state")("health-indicator")();
  static constexpr auto kHealthIndicatorGood = HEALTH_STATE_GOOD;
  static constexpr char kHealthIndicatorGoodString[] = "GOOD";
  static constexpr auto kHealthIndicatorBad = HEALTH_STATE_BAD;
  static constexpr char kHealthIndicatorBadString[] = "BAD";
  static constexpr auto kHealthIndicatorInvalid =
      static_cast<HealthState>(HealthState_MAX + 1);
  static constexpr char kHealthIndicatorInvalidString[] = "UNKNOWN";

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller ('GOOD' case).
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortHealthIndicatorChangedEvent(kInterface1NodeId, kInterface1PortId,
                                      kHealthIndicatorGood),
      &resp));
  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(),
            kHealthIndicatorGoodString);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller ('BAD' case).
  resp.Clear();
  ASSERT_OK(ExecuteOnChange(
      path,
      PortHealthIndicatorChangedEvent(kInterface1NodeId, kInterface1PortId,
                                      kHealthIndicatorBad),
      &resp));
  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(),
            kHealthIndicatorBadString);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller ('UNKNOWN' case).
  resp.Clear();
  ASSERT_OK(ExecuteOnChange(
      path,
      PortHealthIndicatorChangedEvent(kInterface1NodeId, kInterface1PortId,
                                      kHealthIndicatorInvalid),
      &resp));
  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(),
            kHealthIndicatorInvalidString);
}

// Check if the '/interfaces/interface/config/health-indicator' OnPoll
// action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceConfigHealthIndicatorOnPollSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("config")("health-indicator")();
  static constexpr char kHealthIndicatorGoodString[] = "GOOD";
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnPoll(path, &resp));

  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(),
            kHealthIndicatorGoodString);
}

// Check if the '/interfaces/interface/config/health-indicator'
// OnChange action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceConfigHealthIndicatorOnChangeSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("config")("health-indicator")();
  static constexpr auto kHealthIndicatorGood = HealthState::HEALTH_STATE_GOOD;
  static constexpr char kHealthIndicatorGoodString[] = "GOOD";
  static constexpr auto kHealthIndicatorBad = HealthState::HEALTH_STATE_BAD;
  static constexpr char kHealthIndicatorBadString[] = "BAD";
  static constexpr auto kHealthIndicatorUnknown = static_cast<HealthState>(3);
  static constexpr char kHealthIndicatorUnknownString[] = "UNKNOWN";

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller ('GOOD' case).
  ::gnmi::SubscribeResponse resp;
  ASSERT_OK(ExecuteOnChange(
      path,
      PortHealthIndicatorChangedEvent(kInterface1NodeId, kInterface1PortId,
                                      kHealthIndicatorGood),
      &resp));
  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(),
            kHealthIndicatorGoodString);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller ('BAD' case).
  resp.Clear();
  ASSERT_OK(ExecuteOnChange(
      path,
      PortHealthIndicatorChangedEvent(kInterface1NodeId, kInterface1PortId,
                                      kHealthIndicatorBad),
      &resp));
  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(),
            kHealthIndicatorBadString);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller ('UNKNOWN' case).
  resp.Clear();
  ASSERT_OK(ExecuteOnChange(
      path,
      PortHealthIndicatorChangedEvent(kInterface1NodeId, kInterface1PortId,
                                      kHealthIndicatorUnknown),
      &resp));
  // Check that the result of the call is what is expected.
  ASSERT_THAT(resp.update().update(), SizeIs(1));
  EXPECT_EQ(resp.update().update(0).val().string_val(),
            kHealthIndicatorUnknownString);
}

// Check if the '/interfaces/interface/ethernet/config/health-indicator'
// OnUpdate action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceConfigHealthIndicatorOnUpdateSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("config")("health-indicator")();
  static constexpr char kHealthIndicatorBadString[] = "BAD";
  static constexpr auto kHealthIndicatorBad = HealthState::HEALTH_STATE_BAD;
  static constexpr char kHealthIndicatorSomethingString[] = "SOMETHING";
  ::gnmi::SubscribeResponse resp;

  // Set new value.
  SetRequest req;
  ::gnmi::TypedValue val;
  GnmiEventPtr notification;
  val.set_string_val(kHealthIndicatorBadString);
  ASSERT_OK(ExecuteOnUpdate(path, val, &req, &notification));

  // Check that the set request sent via SwitchInterface has correct content.
  ASSERT_THAT(req.requests(), SizeIs(1));
  EXPECT_EQ(req.requests(0).port().health_indicator().state(),
            kHealthIndicatorBad);

  // Check that the notification contains new value.
  ASSERT_NE(notification, nullptr);
  PortHealthIndicatorChangedEvent* event =
      dynamic_cast<PortHealthIndicatorChangedEvent*>(&*notification);
  ASSERT_NE(event, nullptr);
  EXPECT_EQ(event->GetState(), kHealthIndicatorBad);

  // Check reaction to wrong value.
  val.set_string_val(kHealthIndicatorSomethingString);
  EXPECT_THAT(ExecuteOnUpdate(path, val,
                              /* SetValue will not be called */ nullptr,
                              /* Notification will not be called */ nullptr),
              StatusIs(_, _, ContainsRegex("wrong value")));

  // Check reaction to wrong value type.
  ::gnmi::Value wrong_type_val;
  EXPECT_THAT(ExecuteOnUpdate(path, wrong_type_val,
                              /* SetValue will not be called */ nullptr,
                              /* Notification will not be called */ nullptr),
              StatusIs(_, _, ContainsRegex("not a TypedValue message")));
}

// Check if the '/interfaces/interface/ethernet/config/health-indicator'
// OnReplace action works correctly.
TEST_F(YangParseTreeTest,
       InterfacesInterfaceConfigHealthIndicatorOnReplaceSuccess) {
  auto path = GetPath("interfaces")(
      "interface", "interface-1")("config")("health-indicator")();
  static constexpr char kHealthIndicatorBadString[] = "BAD";
  static constexpr auto kHealthIndicatorBad = HealthState::HEALTH_STATE_BAD;
  static constexpr char kHealthIndicatorSomethingString[] = "SOMETHING";
  ::gnmi::SubscribeResponse resp;

  // Set new value.
  SetRequest req;
  ::gnmi::TypedValue val;
  GnmiEventPtr notification;
  val.set_string_val(kHealthIndicatorBadString);
  ASSERT_OK(ExecuteOnReplace(path, val, &req, &notification));

  // Check that the set request sent via SwitchInterface has correct content.
  ASSERT_THAT(req.requests(), SizeIs(1));
  EXPECT_EQ(req.requests(0).port().health_indicator().state(),
            kHealthIndicatorBad);

  // Check that the notification contains new value.
  ASSERT_NE(notification, nullptr);
  PortHealthIndicatorChangedEvent* event =
      dynamic_cast<PortHealthIndicatorChangedEvent*>(&*notification);
  ASSERT_NE(event, nullptr);
  EXPECT_EQ(event->GetState(), kHealthIndicatorBad);

  // Check reaction to wrong value.
  val.set_string_val(kHealthIndicatorSomethingString);
  EXPECT_THAT(ExecuteOnReplace(path, val,
                               /* SetValue will not be called */ nullptr,
                               /* Notification will not be called */ nullptr),
              StatusIs(_, _, ContainsRegex("wrong value")));

  // Check reaction to wrong value type.
  ::gnmi::Value wrong_type_val;
  EXPECT_THAT(ExecuteOnReplace(path, wrong_type_val,
                               /* SetValue will not be called */ nullptr,
                               /* Notification will not be called */ nullptr),
              StatusIs(_, _, ContainsRegex("not a TypedValue message")));
}

// Check if the 'alarms/memory-error' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorOnPollSuccess) {
  auto path = GetPath("components")(
      "component", "chassis-1")("chassis")("alarms")("memory-error")();

  // The test requires chassis component branch to be added.
  AddSubtreeChassis("chassis-1");

  // Mock implementation of RetrieveValue() that sends a response with contents
  // of whole sub-tree (all leafs).
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_memory_error_alarm()->set_description(
                            kAlarmDescription);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_memory_error_alarm()->set_severity(
                            kAlarmSeverityEnum);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_memory_error_alarm()->set_status(
                            kAlarmStatusTrue);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_memory_error_alarm()->set_time_created(
                            kAlarmTimeCreated);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Mock gRPC stream that checks the contents of the 'resp' parameter.
  SubscribeReaderWriterMock stream;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().string_val(),
                            kAlarmDescription);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().string_val(),
                            kAlarmSeverityText);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().bool_val(),
                            kAlarmStatusTrue);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().uint_val(),
                            kAlarmTimeCreated);
                })),
                Return(true)));

  // Find the leaf under test.
  auto* node = GetRoot().FindNodeOrNull(path);
  ASSERT_FALSE(!node) << "Cannot find the requested path.";

  // Get its OnPoll handler and call it.
  const auto& handler = node->GetOnPollHandler();

  // Call the event handler.
  ASSERT_OK(handler(PollEvent(), &stream));
}

// Check if the 'alarms/memory-error' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorOnChangeSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")();

  // The test requires chassis component branch to be added.
  AddSubtreeChassis("chassis-1");

  // Mock gRPC stream that checks the contents of the 'resp' parameter.
  SubscribeReaderWriterMock stream;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().string_val(),
                            kAlarmDescription);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().string_val(),
                            kAlarmSeverityText);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().bool_val(),
                            kAlarmStatusTrue);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().uint_val(),
                            kAlarmTimeCreated);
                })),
                Return(true)));

  // Find the leaf under test.
  auto* node = GetRoot().FindNodeOrNull(path);
  ASSERT_FALSE(!node) << "Cannot find the requested path.";

  // Get its OnChange handler and call it.
  const auto& handler = node->GetOnChangeHandler();

  // Call the event handler.
  ASSERT_OK(
      handler(MemoryErrorAlarm(kAlarmTimeCreated, kAlarmDescription), &stream));
}

// Check if the 'alarms/memory-error/status' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorStatusOnPollSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("memory-error")("status")();
  TestOnPollAlarmLeaf(path, &::gnmi::TypedValue::bool_val,
                      &DataResponse::mutable_memory_error_alarm,
                      &Alarm::set_status, kAlarmStatusTrue);
}

// Check if the 'alarms/memory-error/status' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorStatusOnChangeSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("memory-error")("status")();
  TestOnChangeAlarmLeaf<MemoryErrorAlarm>(path, &::gnmi::TypedValue::bool_val,
                                          kAlarmStatusTrue);
}

// Check if the 'alarms/memory-error/info' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorInfoOnPollSuccess) {
  auto path = GetPath("components")(
      "component", "chassis-1")("chassis")("alarms")("memory-error")("info")();
  TestOnPollAlarmLeaf(path, &::gnmi::TypedValue::string_val,
                      &DataResponse::mutable_memory_error_alarm,
                      &Alarm::set_description, kAlarmDescription);
}

// Check if the 'alarms/memory-error/info' OnChange action works correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorInfoOnChangeSuccess) {
  auto path = GetPath("components")(
      "component", "chassis-1")("chassis")("alarms")("memory-error")("info")();
  TestOnChangeAlarmLeaf<MemoryErrorAlarm>(path, &::gnmi::TypedValue::string_val,
                                          kAlarmDescription);
}

// Check if the 'alarms/memory-error/time-created' OnPoll action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorTimeCreatedOnPollSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("memory-error")("time-created")();
  TestOnPollAlarmLeaf(path, &::gnmi::TypedValue::uint_val,
                      &DataResponse::mutable_memory_error_alarm,
                      &Alarm::set_time_created, kAlarmTimeCreated);
}

// Check if the 'alarms/memory-error/time-created' OnChange action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorTimeCreatedOnChangeSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("memory-error")("time-created")();
  TestOnChangeAlarmLeaf<MemoryErrorAlarm>(path, &::gnmi::TypedValue::uint_val,
                                          kAlarmTimeCreated);
}

// Check if the 'alarms/memory-error/severity' OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorSevrityOnPollSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("memory-error")("severity")();
  TestOnPollAlarmLeaf(path, &::gnmi::TypedValue::string_val,
                      &DataResponse::mutable_memory_error_alarm,
                      &Alarm::set_severity, kAlarmSeverityText,
                      kAlarmSeverityEnum);
}

// Check if the 'alarms/memory-error/severity' OnChange action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsMemoryErrorSeverityOnChangeSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("memory-error")("severity")();
  TestOnChangeAlarmLeaf<MemoryErrorAlarm>(path, &::gnmi::TypedValue::string_val,
                                          kAlarmSeverityText);
}

// Check if the 'alarms/flow-programming-exception' OnPoll action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsFlowProgExcptnOnPollSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")();

  // The test requires chassis component branch to be added.
  AddSubtreeChassis("chassis-1");

  // Mock implementation of RetrieveValue() that sends a response with contents
  // of whole sub-tree (all leafs).
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(
          WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
            DataResponse resp;
            // Set the response.
            resp.mutable_flow_programming_exception_alarm()->set_description(
                kAlarmDescription);
            // Send it to the caller.
            w->Write(resp);
          })),
          Return(::util::OkStatus())))
      .WillOnce(
          DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                  DataResponse resp;
                  // Set the response.
                  resp.mutable_flow_programming_exception_alarm()->set_severity(
                      kAlarmSeverityEnum);
                  // Send it to the caller.
                  w->Write(resp);
                })),
                Return(::util::OkStatus())))
      .WillOnce(
          DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                  DataResponse resp;
                  // Set the response.
                  resp.mutable_flow_programming_exception_alarm()->set_status(
                      kAlarmStatusTrue);
                  // Send it to the caller.
                  w->Write(resp);
                })),
                Return(::util::OkStatus())))
      .WillOnce(DoAll(
          WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
            DataResponse resp;
            // Set the response.
            resp.mutable_flow_programming_exception_alarm()->set_time_created(
                kAlarmTimeCreated);
            // Send it to the caller.
            w->Write(resp);
          })),
          Return(::util::OkStatus())));

  // Mock gRPC stream that checks the contents of the 'resp' parameter.
  SubscribeReaderWriterMock stream;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().string_val(),
                            kAlarmDescription);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().string_val(),
                            kAlarmSeverityText);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().bool_val(),
                            kAlarmStatusTrue);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().uint_val(),
                            kAlarmTimeCreated);
                })),
                Return(true)));

  // Find the leaf under test.
  auto* node = GetRoot().FindNodeOrNull(path);
  ASSERT_FALSE(!node) << "Cannot find the requested path.";

  // Get its OnPoll handler and call it.
  const auto& handler = node->GetOnPollHandler();

  // Call the event handler.
  ASSERT_OK(handler(PollEvent(), &stream));
}

// Check if the 'alarms/flow-programming-exception' OnChange action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsFlowProgExcptnOnChangeSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")();

  // The test requires chassis component branch to be added.
  AddSubtreeChassis("chassis-1");

  // Mock gRPC stream that checks the contents of the 'resp' parameter.
  SubscribeReaderWriterMock stream;
  EXPECT_CALL(stream, Write(_, _))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().string_val(),
                            kAlarmDescription);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().string_val(),
                            kAlarmSeverityText);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().bool_val(),
                            kAlarmStatusTrue);
                })),
                Return(true)))
      .WillOnce(
          DoAll(WithArgs<0>(Invoke([](const ::gnmi::SubscribeResponse& resp) {
                  // Check that the result of the call is what is expected.
                  ASSERT_THAT(resp.update().update(), SizeIs(1));
                  EXPECT_EQ(resp.update().update(0).val().uint_val(),
                            kAlarmTimeCreated);
                })),
                Return(true)));

  // Find the leaf under test.
  auto* node = GetRoot().FindNodeOrNull(path);
  ASSERT_FALSE(!node) << "Cannot find the requested path.";

  // Get its OnChange handler and call it.
  const auto& handler = node->GetOnChangeHandler();

  // Call the event handler.
  ASSERT_OK(handler(
      FlowProgrammingExceptionAlarm(kAlarmTimeCreated, kAlarmDescription),
      &stream));
}

// Check if the 'alarms/flow-programming-exception/status' OnPoll action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsFlowProgExcptnStatusOnPollSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")("status")();
  TestOnPollAlarmLeaf(path, &::gnmi::TypedValue::bool_val,
                      &DataResponse::mutable_flow_programming_exception_alarm,
                      &Alarm::set_status, kAlarmStatusTrue);
}

// Check if the 'alarms/flow-programming-exception/status' OnChange action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsFlowProgExcptnStatusOnChangeSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")("status")();
  TestOnChangeAlarmLeaf<FlowProgrammingExceptionAlarm>(
      path, &::gnmi::TypedValue::bool_val, kAlarmStatusTrue);
}

// Check if the 'alarms/flow-programming-exception/info' OnPoll action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsFlowProgExcptnInfoOnPollSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")("info")();
  TestOnPollAlarmLeaf(path, &::gnmi::TypedValue::string_val,
                      &DataResponse::mutable_flow_programming_exception_alarm,
                      &Alarm::set_description, kAlarmDescription);
}

// Check if the 'alarms/flow-programming-exception/info' OnChange action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsFlowProgExcptnInfoOnChangeSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")("info")();
  TestOnChangeAlarmLeaf<FlowProgrammingExceptionAlarm>(
      path, &::gnmi::TypedValue::string_val, kAlarmDescription);
}

// Check if the 'alarms/flow-programming-exception/time-created' OnPoll action
// works correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsFlowProgExcptnTimeCreatedOnPollSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")("time-created")();
  TestOnPollAlarmLeaf(path, &::gnmi::TypedValue::uint_val,
                      &DataResponse::mutable_flow_programming_exception_alarm,
                      &Alarm::set_time_created, kAlarmTimeCreated);
}

// Check if the 'alarms/flow-programming-exception/time-created' OnChange action
// works correctly.
TEST_F(
    YangParseTreeTest,
    ComponentsComponentChassisAlarmsFlowProgExcptnTimeCreatedOnChangeSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")("time-created")();
  TestOnChangeAlarmLeaf<FlowProgrammingExceptionAlarm>(
      path, &::gnmi::TypedValue::uint_val, kAlarmTimeCreated);
}

// Check if the 'alarms/flow-programming-exception/severity' OnPoll action works
// correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsFlowProgExcptnSevrityOnPollSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")("severity")();
  TestOnPollAlarmLeaf(path, &::gnmi::TypedValue::string_val,
                      &DataResponse::mutable_flow_programming_exception_alarm,
                      &Alarm::set_severity, kAlarmSeverityText,
                      kAlarmSeverityEnum);
}

// Check if the 'alarms/flow-programming-exception/severity' OnChange action
// works correctly.
TEST_F(YangParseTreeTest,
       ComponentsComponentChassisAlarmsFlowProgExcptnSeverityOnChangeSuccess) {
  auto path = GetPath("components")("component", "chassis-1")("chassis")(
      "alarms")("flow-programming-exception")("severity")();
  TestOnChangeAlarmLeaf<FlowProgrammingExceptionAlarm>(
      path, &::gnmi::TypedValue::string_val, kAlarmSeverityText);
}

// Check if all expected handlers are registered
TEST_F(YangParseTreeTest,
       ExpectedRegistrationsTakePlaceInterfacesInterfaceElipsis) {
  // After tree creation only two leafs are defined:
  // /interfaces/interface[name=*]/state/ifindex
  // /interfaces/interface[name=*]/state/name

  // The test requires one interface branch to be added.
  AddSubtreeInterface("interface-1");

  auto path = GetPath("interfaces")("interface")("...")();

  // Find the leaf under test.
  auto* node = GetRoot().FindNodeOrNull(path);
  ASSERT_FALSE(node == nullptr) << "Cannot find the requested path.";

  SubscriptionHandle record(new EventHandlerRecord(
      [](const GnmiEvent& event, GnmiSubscribeStream* stream) {
        return ::util::OkStatus();
      },
      nullptr));

  ASSERT_OK(node->DoOnChangeRegistration(EventHandlerRecordPtr(record)));

  EXPECT_EQ(EventHandlerList<PortOperStateChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            1);
  EXPECT_EQ(EventHandlerList<PortAdminStateChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            1);
  EXPECT_EQ(EventHandlerList<PortSpeedBpsChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            1);
  EXPECT_EQ(EventHandlerList<PortNegotiatedSpeedBpsChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            1);
  EXPECT_EQ(EventHandlerList<PortLacpSystemPriorityChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortMacAddressChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            1);
  EXPECT_EQ(EventHandlerList<PortLacpRouterMacChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortCountersChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            1);
  EXPECT_EQ(EventHandlerList<PortCountersChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            1);
  EXPECT_EQ(EventHandlerList<ConfigHasBeenPushedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<MemoryErrorAlarm>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<FlowProgrammingExceptionAlarm>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
}

// Check if all expected handlers are registered
TEST_F(YangParseTreeTest,
       ExpectedRegistrationsTakePlaceComponentsComponetChassisAlarms) {
  // After tree creation only two leafs are defined:
  // /interfaces/interface[name=*]/state/ifindex
  // /interfaces/interface[name=*]/state/name

  // The test requires chassis component branch to be added.
  AddSubtreeChassis("chassis-1");

  auto path =
      GetPath("components")("component", "chassis-1")("chassis")("alarms")();

  // Find the leaf under test.
  auto* node = GetRoot().FindNodeOrNull(path);
  ASSERT_FALSE(node == nullptr) << "Cannot find the requested path.";

  SubscriptionHandle record(new EventHandlerRecord(
      [](const GnmiEvent& event, GnmiSubscribeStream* stream) {
        return ::util::OkStatus();
      },
      nullptr));

  ASSERT_OK(node->DoOnChangeRegistration(EventHandlerRecordPtr(record)));

  EXPECT_EQ(EventHandlerList<PortOperStateChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortAdminStateChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortSpeedBpsChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortNegotiatedSpeedBpsChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortLacpSystemPriorityChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortMacAddressChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortLacpRouterMacChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortCountersChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<PortCountersChangedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<ConfigHasBeenPushedEvent>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            0);
  EXPECT_EQ(EventHandlerList<MemoryErrorAlarm>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            1);
  EXPECT_EQ(EventHandlerList<FlowProgrammingExceptionAlarm>::GetInstance()
                ->GetNumberOfRegisteredHandlers(),
            1);
}

// Check if the '/qos/interfaces/interface/output/queues/queue/state/name'
// OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       QosInterfacesInterfaceOutputQueuesQueueStateNameOnPollSuccess) {
  auto path = GetPath("qos")("interfaces")("interface", "interface-1")(
      "output")("queues")("queue", "BE1")("state")("name")();

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), kInterface1QueueName);
}

// Check if the '/qos/interfaces/interface/output/queues/queue/state/id' OnPoll
// action works correctly.
TEST_F(YangParseTreeTest,
       QosInterfacesInterfaceOutputQueuesQueueStateIdOnPollSuccess) {
  auto path = GetPath("qos")("interfaces")("interface", "interface-1")(
      "output")("queues")("queue", "BE1")("state")("id")();
  constexpr uint32 kQueueId = 17;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kQueueId.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_qos_counters()->set_queue_id(
                            kQueueId);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kQueueId);
}

// Check if the '/qos/interfaces/interface/output/queues/queue/state/id'
// OnChange action works correctly.
TEST_F(YangParseTreeTest,
       QosInterfacesInterfaceOutputQueuesQueueStateIdOnChangeSuccess) {
  auto path = GetPath("qos")("interfaces")("interface", "interface-1")(
      "output")("queues")("queue", "BE1")("state")("id")();

  // Prepare the structure that stores the counters.
  PortQosCounters counters;
  counters.set_queue_id(kInterface1QueueId);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnChange(path,
                            PortQosCountersChangedEvent(
                                kInterface1NodeId, kInterface1PortId, counters),
                            &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kInterface1QueueId);
}

// Check if /qos/interfaces/interface/output/queues/queue/state/transmit-pkts
// OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       QosInterfacesInterfaceOutputQueuesQueueStateTransmitPktsOnPollSuccess) {
  auto path = GetPath("qos")("interfaces")("interface", "interface-1")(
      "output")("queues")("queue", "BE1")("state")("transmit-pkts")();
  constexpr uint64 kTransmitPkts = 20;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kTransmitPkts.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_qos_counters()->set_out_pkts(
                            kTransmitPkts);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kTransmitPkts);
}

// Check if /qos/interfaces/interface/output/queues/queue/state/transmit-pkts
// OnChange action works correctly.
TEST_F(
    YangParseTreeTest,
    QosInterfacesInterfaceOutputQueuesQueueStateTransmitPktsOnChangeSuccess) {
  auto path = GetPath("qos")("interfaces")("interface", "interface-1")(
      "output")("queues")("queue", "BE1")("state")("transmit-pkts")();
  constexpr uint64 kTransmitPkts = 20;

  // Prepare the structure that stores the counters.
  PortQosCounters counters;
  counters.set_out_pkts(kTransmitPkts);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnChange(path,
                            PortQosCountersChangedEvent(
                                kInterface1NodeId, kInterface1PortId, counters),
                            &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kTransmitPkts);
}

// Check if /qos/interfaces/interface/output/queues/queue/state/transmit-octets
// OnPoll action works correctly.
TEST_F(
    YangParseTreeTest,
    QosInterfacesInterfaceOutputQueuesQueueStateTransmitOctetsOnPollSuccess) {
  auto path = GetPath("qos")("interfaces")("interface", "interface-1")(
      "output")("queues")("queue", "BE1")("state")("transmit-octets")();
  constexpr uint64 kTransmitOctets = 20;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kTransmitOctets.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_qos_counters()->set_out_octets(
                            kTransmitOctets);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kTransmitOctets);
}

// Check if /qos/interfaces/interface/output/queues/queue/state/transmit-octets
// OnChange action works correctly.
TEST_F(
    YangParseTreeTest,
    QosInterfacesInterfaceOutputQueuesQueueStateTransmitOctetsOnChangeSuccess) {
  auto path = GetPath("qos")("interfaces")("interface", "interface-1")(
      "output")("queues")("queue", "BE1")("state")("transmit-octets")();
  constexpr uint64 kTransmitOctets = 20;

  // Prepare the structure that stores the counters.
  PortQosCounters counters;
  counters.set_out_octets(kTransmitOctets);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnChange(path,
                            PortQosCountersChangedEvent(
                                kInterface1NodeId, kInterface1PortId, counters),
                            &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kTransmitOctets);
}

// Check if /qos/interfaces/interface/output/queues/queue/state/dropped-pkts
// OnPoll action works correctly.
TEST_F(YangParseTreeTest,
       QosInterfacesInterfaceOutputQueuesQueueStateDroppedPktsOnPollSuccess) {
  auto path = GetPath("qos")("interfaces")("interface", "interface-1")(
      "output")("queues")("queue", "BE1")("state")("dropped-pkts")();
  constexpr uint64 kDroppedPkts = 20;

  // Mock implementation of RetrieveValue() that sends a response set to
  // kDroppedPkts.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(DoAll(WithArg<2>(Invoke([](WriterInterface<DataResponse>* w) {
                        DataResponse resp;
                        // Set the response.
                        resp.mutable_port_qos_counters()->set_out_dropped_pkts(
                            kDroppedPkts);
                        // Send it to the caller.
                        w->Write(resp);
                      })),
                      Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kDroppedPkts);
}

// Check if /qos/interfaces/interface/output/queues/queue/state/dropped-pkts
// OnChange action works correctly.
TEST_F(YangParseTreeTest,
       QosInterfacesInterfaceOutputQueuesQueueStateDroppedPktsOnChangeSuccess) {
  auto path = GetPath("qos")("interfaces")("interface", "interface-1")(
      "output")("queues")("queue", "BE1")("state")("dropped-pkts")();
  constexpr uint64 kDroppedPkts = 20;

  // Prepare the structure that stores the counters.
  PortQosCounters counters;
  counters.set_out_dropped_pkts(kDroppedPkts);

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnChange(path,
                            PortQosCountersChangedEvent(
                                kInterface1NodeId, kInterface1PortId, counters),
                            &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().uint_val(), kDroppedPkts);
}

// Check if /debug/nodes/node/packet-io/debug-string
// OnPoll action works correctly.
TEST_F(YangParseTreeTest, DebugNodesNodePacketIoDebugStringOnPollSuccess) {
  auto path = GetPath("debug")("nodes")(
      "node", "node-1")("packet-io")("debug-string")();
  constexpr char kTestString[] = "test string";

  // Mock implementation of RetrieveValue() that sends a response set to
  // kDroppedPkts.
  EXPECT_CALL(switch_, RetrieveValue(_, _, _, _))
      .WillOnce(
          DoAll(WithArg<2>(Invoke([&](WriterInterface<DataResponse>* w) {
                  DataResponse resp;
                  // Set the response.
                  resp.mutable_node_packetio_debug_info()->set_debug_string(
                      kTestString);
                  // Send it to the caller.
                  w->Write(resp);
                })),
                Return(::util::OkStatus())));

  // Call the event handler. 'resp' will contain the message that is sent to the
  // controller.
  ::gnmi::SubscribeResponse resp;
  EXPECT_OK(ExecuteOnPoll(path, &resp));

  // Check that the result of the call is what is expected.
  ASSERT_EQ(resp.update().update_size(), 1);
  EXPECT_EQ(resp.update().update(0).val().string_val(), kTestString);
}

}  // namespace hal
}  // namespace stratum
