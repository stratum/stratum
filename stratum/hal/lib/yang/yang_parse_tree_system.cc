// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Implements the YangParseTreePaths::AddSubtreeSystem() method and its
// supporting functions.

#include "stratum/hal/lib/yang/yang_parse_tree_paths.h"

#include "gnmi/gnmi.pb.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/yang/yang_parse_tree_helpers.h"
#include "stratum/hal/lib/yang/yang_parse_tree.h"
#include "stratum/hal/lib/common/utils.h"

namespace stratum {
namespace hal {

using namespace stratum::hal::yang::helpers;

namespace {

////////////////////////////////////////////////////////////////////////////////
// /system/logging/console/config/severity
void SetUpSystemLoggingConsoleConfigSeverity(
    LoggingConfig logging_config, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = [logging_config](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(
        GetResponse(path, ConvertLogSeverityToString(logging_config)), stream);
  };

  auto on_set_functor =
      [node, tree](const ::gnmi::Path& path,
                   const ::google::protobuf::Message& val,
                   CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    LoggingConfig logging_config;
    RETURN_IF_ERROR(
        ConvertStringToLogSeverity(typed_val->string_val(), &logging_config));

    // Set the value.
    CHECK_RETURN_IF_FALSE(SetLogLevel(logging_config))
        << "Could not set new log level (" << logging_config.first << ", "
        << logging_config.second << ").";

    // Update the YANG parse tree.
    auto poll_functor = [logging_config](const GnmiEvent& event,
                                         const ::gnmi::Path& path,
                                         GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when it
      // was configured!
      return SendResponse(
          GetResponse(path, ConvertLogSeverityToString(logging_config)),
          stream);
    };
    node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

    // Trigger change notification.
    tree->SendNotification(GnmiEventPtr(new ConsoleLogSeverityChangedEvent(
        logging_config.first, logging_config.second)));

    return ::util::OkStatus();
  };
  auto register_functor = RegisterFunc<ConsoleLogSeverityChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      &ConsoleLogSeverityChangedEvent::GetState, ConvertLogSeverityToString);
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /system/logging/console/state/severity
void SetUpSystemLoggingConsoleStateSeverity(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = [](const GnmiEvent& event, const ::gnmi::Path& path,
                         GnmiSubscribeStream* stream) -> ::util::Status {
    return SendResponse(
        GetResponse(path, ConvertLogSeverityToString(GetCurrentLogLevel())),
        stream);
  };
  auto register_functor = RegisterFunc<ConsoleLogSeverityChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      &ConsoleLogSeverityChangedEvent::GetState, ConvertLogSeverityToString);
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

} // namespace

////////////////////////
//  AddSubtreeSystem  //
////////////////////////

void YangParseTreePaths::AddSubtreeSystem(YangParseTree* tree) {
  LoggingConfig log_level = GetCurrentLogLevel();
  TreeNode* node = tree->AddNode(
      GetPath("system")("logging")("console")("config")("severity")());
  SetUpSystemLoggingConsoleConfigSeverity(log_level, node, tree);

  node = tree->AddNode(
      GetPath("system")("logging")("console")("state")("severity")());
  SetUpSystemLoggingConsoleStateSeverity(node, tree);
}

} // namespace hal
} // namespace stratum
