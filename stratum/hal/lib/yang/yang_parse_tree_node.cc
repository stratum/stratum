// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Implements the YangParseTreePaths::AddSubtreeNode() method and its
// supporting functions.

#include "absl/strings/str_format.h"
#include "gnmi/gnmi.pb.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/yang/yang_parse_tree.h"
#include "stratum/hal/lib/yang/yang_parse_tree_component.h"
#include "stratum/hal/lib/yang/yang_parse_tree_helpers.h"
#include "stratum/hal/lib/yang/yang_parse_tree_paths.h"

namespace stratum {
namespace hal {

using namespace stratum::hal::yang::component;
using namespace stratum::hal::yang::helpers;

namespace {

////////////////////////////////////////////////////////////////////////////////
// /debug/nodes/node[name=<name>]/packet-io/debug-string
void SetUpDebugNodesNodePacketIoDebugString(uint64 node_id, TreeNode* node,
                                            YangParseTree* tree) {
  // Regular method using a template cannot be used to get the OnPoll functor as
  // std::string fields are treated differently by the PROTO-to-C++ generator:
  // the getter returns "const std::string&" instead of "string" which leads to
  // the template compilation error.
  auto poll_functor = [node_id, tree](const GnmiEvent& event,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_node_packetio_debug_info();
    request->set_node_id(node_id);
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_node_packetio_debug_info()) return false;
      resp = in.node_packetio_debug_info().debug_string();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/integrated-circuit/config/node-id
void SetUpComponentsComponentIntegratedCircuitConfigNodeId(
    uint64 node_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = [node_id](const GnmiEvent& event,
                                const ::gnmi::Path& path,
                                GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, node_id), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/integrated-circuit/state/node-id
void SetUpComponentsComponentIntegratedCircuitStateNodeId(uint64 node_id,
                                                          TreeNode* node,
                                                          YangParseTree* tree) {
  auto poll_functor = [node_id](const GnmiEvent& event,
                                const ::gnmi::Path& path,
                                GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, node_id), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

}  // namespace

//////////////////////
//  AddSubtreeNode  //
//////////////////////

void YangParseTreePaths::AddSubtreeNode(const Node& node, YangParseTree* tree) {
  // No need to lock the mutex - it is locked by method calling this one.
  const std::string& name =
      node.name().empty() ? absl::StrFormat("node-%d", node.id()) : node.name();
  TreeNode* tree_node = tree->AddNode(
      GetPath("debug")("nodes")("node", name)("packet-io")("debug-string")());
  SetUpDebugNodesNodePacketIoDebugString(node.id(), tree_node, tree);
  tree_node = tree->AddNode(GetPath("components")(
      "component", name)("integrated-circuit")("config")("node-id")());
  SetUpComponentsComponentIntegratedCircuitConfigNodeId(node.id(), tree_node,
                                                        tree);
  tree_node = tree->AddNode(GetPath("components")(
      "component", name)("integrated-circuit")("state")("node-id")());
  SetUpComponentsComponentIntegratedCircuitStateNodeId(node.id(), tree_node,
                                                       tree);
  tree_node = tree->AddNode(
      GetPath("components")("component", name)("state")("type")());
  SetUpComponentsComponentStateType("INTEGRATED_CIRCUIT", tree_node);
  tree_node = tree->AddNode(
      GetPath("components")("component", name)("state")("part-no")());
  SetUpComponentsComponentStatePartNo(node.id(), tree_node, tree);
  tree_node = tree->AddNode(
      GetPath("components")("component", name)("state")("mfg-name")());
  SetUpComponentsComponentStateMfgName(node.id(), tree_node, tree);
  tree_node = tree->AddNode(
      GetPath("components")("component", name)("state")("description")());
  SetUpComponentsComponentStateDescription(node.name(), tree_node);
}

}  // namespace hal
}  // namespace stratum
