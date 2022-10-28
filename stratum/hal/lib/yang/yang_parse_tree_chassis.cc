// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Implements the YangParseTreePaths::AddSubtreeChassis() method and its
// supporting functions.

#include "stratum/hal/lib/yang/yang_parse_tree_paths.h"

#include "gnmi/gnmi.pb.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/yang/yang_parse_tree_component.h"
#include "stratum/hal/lib/yang/yang_parse_tree_helpers.h"
#include "stratum/hal/lib/yang/yang_parse_tree.h"
#include "stratum/hal/lib/common/utils.h"

namespace stratum {
namespace hal {

using namespace stratum::hal::yang::component;
using namespace stratum::hal::yang::helpers;

namespace {

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error
void SetUpComponentsComponentChassisAlarmsMemoryError(
    TreeNode* node, YangParseTree* tree) {
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnChangeRegistration(register_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error/status
void SetUpComponentsComponentChassisAlarmsMemoryErrorStatus(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::memory_error_alarm,
      &DataResponse::has_memory_error_alarm,
      &DataRequest::Request::mutable_memory_error_alarm, &Alarm::status);
  auto on_change_functor = GetOnChangeFunctor(&MemoryErrorAlarm::GetStatus);
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error/time-created
void SetUpComponentsComponentChassisAlarmsMemoryErrorTimeCreated(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::memory_error_alarm,
      &DataResponse::has_memory_error_alarm,
      &DataRequest::Request::mutable_memory_error_alarm, &Alarm::time_created);
  auto on_change_functor =
      GetOnChangeFunctor(&MemoryErrorAlarm::GetTimeCreated);
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error/info
void SetUpComponentsComponentChassisAlarmsMemoryErrorInfo(
    TreeNode* node, YangParseTree* tree) {
  // Regular method using a template cannot be used to get the OnPoll functor as
  // std::string fields are treated differently by the PROTO-to-C++ generator:
  // the getter returns "const std::string&" instead of "string" which leads to
  // the template compilation error.
  auto poll_functor = [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    *(req.add_requests()->mutable_memory_error_alarm()) =
        DataRequest::Request::Chassis();
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_memory_error_alarm()) return false;
      resp = in.memory_error_alarm().description();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(/* node_id= */ 0, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };

  auto on_change_functor = GetOnChangeFunctor(&MemoryErrorAlarm::GetInfo);
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error/severity
void SetUpComponentsComponentChassisAlarmsMemoryErrorSeverity(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(tree, &DataResponse::memory_error_alarm,
                       &DataResponse::has_memory_error_alarm,
                       &DataRequest::Request::mutable_memory_error_alarm,
                       &Alarm::severity, ConvertAlarmSeverityToString);
  auto on_change_functor = GetOnChangeFunctor(&MemoryErrorAlarm::GetSeverity);
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/flow-programming-exception
void SetUpComponentsComponentChassisAlarmsFlowProgrammingException(
    TreeNode* node, YangParseTree* tree) {
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnChangeRegistration(register_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/
//     flow-programming-exception/status
void SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionStatus(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::flow_programming_exception_alarm,
      &DataResponse::has_flow_programming_exception_alarm,
      &DataRequest::Request::mutable_flow_programming_exception_alarm,
      &Alarm::status);
  auto on_change_functor =
      GetOnChangeFunctor(&FlowProgrammingExceptionAlarm::GetStatus);
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/
//     flow-programming-exception/time-created
void SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionTimeCreated(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::flow_programming_exception_alarm,
      &DataResponse::has_flow_programming_exception_alarm,
      &DataRequest::Request::mutable_flow_programming_exception_alarm,
      &Alarm::time_created);
  auto on_change_functor =
      GetOnChangeFunctor(&FlowProgrammingExceptionAlarm::GetTimeCreated);
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/
//     flow-programming-exception/info
void SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionInfo(
    TreeNode* node, YangParseTree* tree) {
  // Regular method using a template cannot be used to get the OnPoll functor as
  // std::string fields are treated differently by the PROTO-to-C++ generator:
  // the getter returns "const std::string&" instead of "string" which leads to
  // the template compilation error.
  auto poll_functor = [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    *(req.add_requests()->mutable_flow_programming_exception_alarm()) =
        DataRequest::Request::Chassis();
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_flow_programming_exception_alarm()) return false;
      resp = in.flow_programming_exception_alarm().description();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(/* node_id= */ 0, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };

  auto on_change_functor =
      GetOnChangeFunctor(&FlowProgrammingExceptionAlarm::GetInfo);
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/
//     flow-programming-exception/severity
void SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionSeverity(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::flow_programming_exception_alarm,
      &DataResponse::has_flow_programming_exception_alarm,
      &DataRequest::Request::mutable_flow_programming_exception_alarm,
      &Alarm::severity, ConvertAlarmSeverityToString);
  auto on_change_functor =
      GetOnChangeFunctor(&FlowProgrammingExceptionAlarm::GetSeverity);
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

} // namespace

/////////////////////////
//  AddSubtreeChassis  //
/////////////////////////

void YangParseTreePaths::AddSubtreeChassis(
    const Chassis& chassis, YangParseTree* tree) {
  const std::string& name = chassis.name().empty() ? "chassis" : chassis.name();
  TreeNode* node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("memory-error")());
  SetUpComponentsComponentChassisAlarmsMemoryError(node, tree);
  node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("memory-error")("status")());
  SetUpComponentsComponentChassisAlarmsMemoryErrorStatus(node, tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("memory-error")("time-created")());
  SetUpComponentsComponentChassisAlarmsMemoryErrorTimeCreated(node, tree);
  node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("memory-error")("info")());
  SetUpComponentsComponentChassisAlarmsMemoryErrorInfo(node, tree);
  node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("memory-error")("severity")());
  SetUpComponentsComponentChassisAlarmsMemoryErrorSeverity(node, tree);

  node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("flow-programming-exception")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingException(node, tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("flow-programming-exception")("status")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionStatus(node,
                                                                      tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("flow-programming-exception")("time-created")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionTimeCreated(
      node, tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("flow-programming-exception")("info")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionInfo(node, tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("flow-programming-exception")("severity")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionSeverity(node,
                                                                        tree);
  node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("state")("description")());
  SetUpComponentsComponentStateDescription(chassis.name(), node);
}

}  // namespace hal
}  // namespace stratum
