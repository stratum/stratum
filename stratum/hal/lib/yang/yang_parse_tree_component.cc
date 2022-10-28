// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Implements the SetUpComponents support functions, which are used by
// several of the YangParseTreePaths methods.

#include "stratum/hal/lib/yang/yang_parse_tree_component.h"

#include "stratum/hal/lib/yang/yang_parse_tree_helpers.h"

namespace stratum {
namespace hal {
namespace yang {
namespace component {

using namespace stratum::hal::yang::helpers;

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/config/name
void SetUpComponentsComponentConfigName(
    const std::string& name, TreeNode* node) {
  auto poll_functor = [name](const GnmiEvent& /*event*/,
                             const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, name), stream);
  };

  // This /config node represents the component name in the configuration tree,
  // so it doesn't support OnChange/OnUpdate/OnReplace until the yang tree
  // supports nodes renaming.
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/name
void SetUpComponentsComponentName(const std::string& name, TreeNode* node) {
  auto poll_functor = [name](const GnmiEvent& /*event*/,
                             const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, name), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/state/type
void SetUpComponentsComponentStateType(
    const std::string& type, TreeNode* node) {
  auto poll_functor = [type](const GnmiEvent& /*event*/,
                             const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, type), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/state/description
void SetUpComponentsComponentStateDescription(
    const std::string& description, TreeNode* node) {
  auto poll_functor = [description](const GnmiEvent& /*event*/,
                                    const ::gnmi::Path& path,
                                    GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, description), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/state/part-no
void SetUpComponentsComponentStatePartNo(
    uint64 node_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = [node_id, tree](const GnmiEvent& event,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_node_info();
    request->set_node_id(node_id);
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_node_info()) return false;
      resp = in.node_info().chip_name();
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
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/state/mfg-name
void SetUpComponentsComponentStateMfgName(
    uint64 node_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = [node_id, tree](const GnmiEvent& event,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_node_info();
    request->set_node_id(node_id);
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_node_info()) return false;
      resp = in.node_info().vendor_name();
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
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

} // namespace component
} // namespace yang
} // namespace hal
} // namespace stratum
