// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/yang/yang_parse_tree_paths.h"

#include <utility>
#include <vector>

#include "gnmi/gnmi.pb.h"
#include "openconfig/openconfig.pb.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/common/openconfig_converter.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/hal/lib/yang/yang_parse_tree_component.h"
#include "stratum/hal/lib/yang/yang_parse_tree_helpers.h"

namespace stratum {
namespace hal {

using namespace stratum::hal::yang::helpers;

namespace {

////////////////////////////////////////////////////////////////////////////////
// /
void SetUpRoot(TreeNode* node, YangParseTree* tree) {
  auto poll_functor = UnsupportedFunc();
  auto on_change_functor = UnsupportedFunc();
  auto on_replace_functor =
      [](const ::gnmi::Path& path, const ::google::protobuf::Message& val,
         CopyOnWriteChassisConfig* config) -> ::util::Status {
    auto* typed_value = static_cast<const gnmi::TypedValue*>(&val);
    if (!typed_value) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Not a TypedValue!";
    }
    if (typed_value->value_case() != gnmi::TypedValue::kBytesVal) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Expects a bytes stream!";
    }
    openconfig::Device in;
    // Deserialize the input proto to OpenConfig device format.
    if (in.ParseFromString(typed_value->bytes_val())) {
      // Convert the input proto into the internal format.
      ASSIGN_OR_RETURN(*config->writable(),
                       OpenconfigConverter::OcDeviceToChassisConfig(in));
    } else {
      // Try parse it with ChassisConfig format.
      RETURN_IF_ERROR(
          ParseProtoFromString(typed_value->bytes_val(), config->writable()));
    }
    return ::util::OkStatus();
  };
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnReplaceHandler(on_replace_functor);
}

}  // namespace

void YangParseTreePaths::AddSubtreeInterfaceFromTrunk(
    const std::string& name, uint64 node_id, uint32 port_id,
    const NodeConfigParams& node_config, YangParseTree* tree) {
  AddSubtreeInterface(name, node_id, port_id, node_config, tree);
}

void YangParseTreePaths::AddSubtreeAllInterfaces(YangParseTree* tree) {
  // Add support for "/interfaces/interface[name=*]/state/ifindex".
  tree->AddNode(GetPath("interfaces")("interface", "*")("state")("ifindex")())
      ->SetOnChangeRegistration(
          [tree](const EventHandlerRecordPtr& record)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Subscribing to a wildcard node means that all matching nodes
                // have to be registered for received events.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("ifindex")(),
                    [&record](const TreeNode& node) {
                      return node.DoOnChangeRegistration(record);
                    });
                return status;
              })
      ->SetOnChangeHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream) { return ::util::OkStatus(); })
      ->SetOnPollHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Polling a wildcard node means that all matching nodes have to
                // be polled.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("ifindex")(),
                    [&event, &stream](const TreeNode& leaf) {
                      return (leaf.GetOnPollHandler())(event, stream);
                    });
                // Notify the client that all nodes have been processed.
                APPEND_STATUS_IF_ERROR(
                    status, YangParseTreePaths::SendEndOfSeriesMessage(stream));
                return status;
              });
  // Add support for "/interfaces/interface[name=*]/state/name".
  tree->AddNode(GetPath("interfaces")("interface", "*")("state")("name")())
      ->SetOnChangeRegistration(
          [tree](const EventHandlerRecordPtr& record)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Subscribing to a wildcard node means that all matching nodes
                // have to be registered for received events.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("name")(),
                    [&record](const TreeNode& node) {
                      return node.DoOnChangeRegistration(record);
                    });
                return status;
              })
      ->SetOnChangeHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream) { return ::util::OkStatus(); })
      ->SetOnPollHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Polling a wildcard node means that all matching nodes have to
                // be polled.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("name")(),
                    [&event, &stream](const TreeNode& leaf) {
                      return (leaf.GetOnPollHandler())(event, stream);
                    });
                // Notify the client that all nodes have been processed.
                APPEND_STATUS_IF_ERROR(
                    status, YangParseTreePaths::SendEndOfSeriesMessage(stream));
                return status;
              });
  // Add support for "/interfaces/interface[name=*]/state/counters".
  tree->AddNode(GetPath("interfaces")("interface", "*")("state")("counters")())
      ->SetOnChangeRegistration(
          [tree](const EventHandlerRecordPtr& record)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Subscribing to a wildcard node means that all matching nodes
                // have to be registered for received events.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("counters")(),
                    [&record](const TreeNode& node) {
                      return node.DoOnChangeRegistration(record);
                    });
                return status;
              })
      ->SetOnChangeHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream) { return ::util::OkStatus(); })
      ->SetOnPollHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Polling a wildcard node means that all matching nodes have to
                // be polled.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("counters")(),
                    [&event, &stream](const TreeNode& leaf) {
                      return (leaf.GetOnPollHandler())(event, stream);
                    });
                // Notify the client that all nodes have been processed.
                APPEND_STATUS_IF_ERROR(
                    status, YangParseTreePaths::SendEndOfSeriesMessage(stream));
                return status;
              })
      ->SetOnTimerHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Polling a wildcard node means that all matching nodes have to
                // be polled.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("counters")(),
                    [&event, &stream](const TreeNode& leaf) {
                      return (leaf.GetOnPollHandler())(event, stream);
                    });
                // Notify the client that all nodes have been processed.
                APPEND_STATUS_IF_ERROR(
                    status, YangParseTreePaths::SendEndOfSeriesMessage(stream));
                return status;
              });

  auto interfaces_on_chage_reg =
      [tree](const EventHandlerRecordPtr& record)
          EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
            // Subscribing to a wildcard node means that all matching nodes
            // have to be registered for received events.
            auto status = tree->PerformActionForAllNonWildcardNodes(
                GetPath("interfaces")("interface")(), gnmi::Path(),
                [&record](const TreeNode& node) {
                  return node.DoOnChangeRegistration(record);
                });
            return status;
          };  // NOLINT(readability/braces)

  auto interfaces_on_poll =
      [tree](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream)
          EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
            // Polling a wildcard node means that all matching nodes have to
            // be polled.
            auto status = tree->PerformActionForAllNonWildcardNodes(
                GetPath("interfaces")("interface")(), gnmi::Path(),
                [&event, &stream](const TreeNode& node) {
                  return (node.GetOnPollHandler())(event, stream);
                });
            // Notify the client that all nodes have been processed.
            APPEND_STATUS_IF_ERROR(
                status, YangParseTreePaths::SendEndOfSeriesMessage(stream));
            return status;
          };  // NOLINT(readability/braces)

  // Add support for "/interfaces/interface/...".
  tree->AddNode(GetPath("interfaces")("interface")("...")())
      ->SetOnChangeRegistration(interfaces_on_chage_reg)
      ->SetOnChangeHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream) { return ::util::OkStatus(); })
      ->SetOnPollHandler(interfaces_on_poll);

  // Add support for "/interfaces/interface/*".
  tree->AddNode(GetPath("interfaces")("interface")("*")())
      ->SetOnChangeRegistration(interfaces_on_chage_reg)
      ->SetOnChangeHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream) { return ::util::OkStatus(); })
      ->SetOnPollHandler(interfaces_on_poll);
}

void YangParseTreePaths::AddSubtreeAllComponents(YangParseTree* tree) {
  auto on_poll_names =
      [tree](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream)
          EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
            // Execute OnPollHandler and send to the stream.
            auto execute_poll = [&event, &stream](const TreeNode& leaf) {
              return (leaf.GetOnPollHandler())(event, stream);
            };

            // Recursively process on-poll.
            auto status = tree->PerformActionForAllNonWildcardNodes(
                GetPath("components")("component")(), GetPath("name")(),
                execute_poll);

            // Notify the client that all nodes have been processed.
            APPEND_STATUS_IF_ERROR(
                status, YangParseTreePaths::SendEndOfSeriesMessage(stream));

            return status;
          };  // NOLINT(readability/braces)
  auto on_change_functor = UnsupportedFunc();

  // Add support for "/components/component[name=*]/name".
  tree->AddNode(GetPath("components")("component", "*")("name")())
      ->SetOnPollHandler(on_poll_names)
      ->SetOnChangeHandler(on_change_functor);

  auto on_poll_all_components =
      [tree](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream)
          EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
            // Execute OnPollHandler and send to the stream.
            auto execute_poll = [&event, &stream](const TreeNode& leaf) {
              return (leaf.GetOnPollHandler())(event, stream);
            };

            // Recursively process on-poll.
            auto status = tree->PerformActionForAllNonWildcardNodes(
                GetPath("components")("component")(), gnmi::Path(),
                execute_poll);

            // Notify the client that all nodes have been processed.
            APPEND_STATUS_IF_ERROR(
                status, YangParseTreePaths::SendEndOfSeriesMessage(stream));

            return status;
          };  // NOLINT(readability/braces)

  // Add support for "/components/component/*".
  tree->AddNode(GetPath("components")("component")("*")())
      ->SetOnPollHandler(on_poll_all_components)
      ->SetOnChangeHandler(on_change_functor);

  // Add support for
  // "/components/component[name=*]/integrated-circuit/state/node-id".
  tree->AddNode(GetPath("components")("component", "*")("integrated-circuit")(
                    "state")("node-id")())
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnPollHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Polling a wildcard node means that all matching nodes have to
                // be polled.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("components")("component")(),
                    GetPath("integrated-circuit")("state")("node-id")(),
                    [&event, &stream](const TreeNode& leaf) {
                      return (leaf.GetOnPollHandler())(event, stream);
                    });
                // Notify the client that all nodes have been processed.
                APPEND_STATUS_IF_ERROR(
                    status, YangParseTreePaths::SendEndOfSeriesMessage(stream));
                return status;
              });
}

void YangParseTreePaths::AddRoot(YangParseTree* tree) {
  // Add support for "/"
  SetUpRoot(tree->AddNode(GetPath()()), tree);
}

// A helper method that handles sending a message that marks the end of series
// of update messages.
::util::Status YangParseTreePaths::SendEndOfSeriesMessage(
    GnmiSubscribeStream* stream) {
  // Notify the client that all nodes have been processed.
  ::gnmi::SubscribeResponse resp;
  resp.set_sync_response(true);
  return SendResponse(resp, stream);
}

}  // namespace hal
}  // namespace stratum
