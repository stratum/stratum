// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Implements the YangParseTreePaths::AddSubtreeInterfaceFromOptical()
// method and its supporting functions.

#include "stratum/hal/lib/yang/yang_parse_tree_paths.h"

#include "absl/strings/str_format.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/hal/lib/yang/yang_parse_tree.h"
#include "stratum/hal/lib/yang/yang_parse_tree_component.h"
#include "stratum/hal/lib/yang/yang_parse_tree_helpers.h"

namespace stratum {
namespace hal {

using namespace stratum::hal::yang::component;
using namespace stratum::hal::yang::helpers;

namespace {

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/frequency
void SetUpComponentsComponentOpticalChannelStateFrequency(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor =
      GetOnPollFunctor(module, network_interface, tree,
                       &OpticalTransceiverInfo::frequency, &ConvertHzToMHz);
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/config/frequency
void SetUpComponentsComponentOpticalChannelConfigFrequency(
    uint64 initial_value, TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = [initial_value](const GnmiEvent& /*event*/,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
    // Use MHz for OpenConfig model.
    return SendResponse(GetResponse(path, ConvertHzToMHz(initial_value)),
                        stream);
  };

  auto on_set_functor =
      [module, network_interface, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    auto typed_value = static_cast<const gnmi::TypedValue*>(&val);
    if (!typed_value) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Not a TypedValue!";
    }
    if (typed_value->value_case() != gnmi::TypedValue::kUintVal) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Expects a uint64 value!";
    }

    // Converts MHz to HZ since OpenConfig uses MHz.
    uint64 uint_val = ConvertMHzToHz(typed_value->uint_val());
    RETURN_IF_ERROR(SetValue(module, network_interface, tree,
                             &OpticalTransceiverInfo::set_frequency, uint_val));

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (auto& optical_port :
         *new_config->mutable_optical_network_interfaces()) {
      if (optical_port.module() == module &&
          optical_port.network_interface() == network_interface) {
        optical_port.set_frequency(uint_val);
        break;
      }
    }

    auto poll_functor = [uint_val](const GnmiEvent& /*event*/,
                                   const ::gnmi::Path& path,
                                   GnmiSubscribeStream* stream) {
      return SendResponse(GetResponse(path, uint_val), stream);
    };
    node->SetOnPollHandler(poll_functor)->SetOnTimerHandler(poll_functor);
    return ::util::OkStatus();
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/input-power/instant
void SetUpComponentsComponentOpticalChannelStateInputPowerInstant(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree, &OpticalTransceiverInfo::has_input_power,
      &OpticalTransceiverInfo::input_power,
      &OpticalTransceiverInfo::Power::instant, &ConvertDoubleToDecimal64OrDie);

  auto register_functor = RegisterFunc<OpticalInputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalInputPowerChangedEvent::GetInstant,
      &ConvertDoubleToDecimal64OrDie);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/input-power/avg
void SetUpComponentsComponentOpticalChannelStateInputPowerAvg(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree, &OpticalTransceiverInfo::has_input_power,
      &OpticalTransceiverInfo::input_power, &OpticalTransceiverInfo::Power::avg,
      &ConvertDoubleToDecimal64OrDie);

  auto register_functor = RegisterFunc<OpticalInputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalInputPowerChangedEvent::GetAvg,
      &ConvertDoubleToDecimal64OrDie);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/input-power/interval
void SetUpComponentsComponentOpticalChannelStateInputPowerInterval(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree, &OpticalTransceiverInfo::has_input_power,
      &OpticalTransceiverInfo::input_power,
      &OpticalTransceiverInfo::Power::interval, &DontProcess<uint64>);
  auto register_functor = RegisterFunc<OpticalInputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalInputPowerChangedEvent::GetInterval,
      &DontProcess<uint64>);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/input-power/max
void SetUpComponentsComponentOpticalChannelStateInputPowerMax(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree, &OpticalTransceiverInfo::has_input_power,
      &OpticalTransceiverInfo::input_power, &OpticalTransceiverInfo::Power::max,
      &ConvertDoubleToDecimal64OrDie);

  auto register_functor = RegisterFunc<OpticalInputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalInputPowerChangedEvent::GetMax,
      &ConvertDoubleToDecimal64OrDie);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/input-power/max-time
void SetUpComponentsComponentOpticalChannelStateInputPowerMaxTime(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree, &OpticalTransceiverInfo::has_input_power,
      &OpticalTransceiverInfo::input_power,
      &OpticalTransceiverInfo::Power::max_time, &DontProcess<uint64>);

  auto register_functor = RegisterFunc<OpticalInputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalInputPowerChangedEvent::GetMaxTime,
      &DontProcess<uint64>);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/input-power/min
void SetUpComponentsComponentOpticalChannelStateInputPowerMin(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree, &OpticalTransceiverInfo::has_input_power,
      &OpticalTransceiverInfo::input_power, &OpticalTransceiverInfo::Power::min,
      &ConvertDoubleToDecimal64OrDie);

  auto register_functor = RegisterFunc<OpticalInputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalInputPowerChangedEvent::GetMin,
      &ConvertDoubleToDecimal64OrDie);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/input-power/min-time
void SetUpComponentsComponentOpticalChannelStateInputPowerMinTime(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree, &OpticalTransceiverInfo::has_input_power,
      &OpticalTransceiverInfo::input_power,
      &OpticalTransceiverInfo::Power::min_time, &DontProcess<uint64>);

  auto register_functor = RegisterFunc<OpticalInputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalInputPowerChangedEvent::GetMinTime,
      &DontProcess<uint64>);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/output-power/instant
void SetUpComponentsComponentOpticalChannelStateOutputPowerInstant(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree,
      &OpticalTransceiverInfo::has_output_power,
      &OpticalTransceiverInfo::output_power,
      &OpticalTransceiverInfo::Power::instant, &ConvertDoubleToDecimal64OrDie);

  auto register_functor = RegisterFunc<OpticalOutputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalOutputPowerChangedEvent::GetInstant,
      &ConvertDoubleToDecimal64OrDie);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/output-power/avg
void SetUpComponentsComponentOpticalChannelStateOutputPowerAvg(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree,
      &OpticalTransceiverInfo::has_output_power,
      &OpticalTransceiverInfo::output_power,
      &OpticalTransceiverInfo::Power::avg, &ConvertDoubleToDecimal64OrDie);

  auto register_functor = RegisterFunc<OpticalOutputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalOutputPowerChangedEvent::GetAvg,
      &ConvertDoubleToDecimal64OrDie);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/output-power
// /interval
void SetUpComponentsComponentOpticalChannelStateOutputPowerInterval(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree,
      &OpticalTransceiverInfo::has_output_power,
      &OpticalTransceiverInfo::output_power,
      &OpticalTransceiverInfo::Power::interval, &DontProcess<uint64>);

  auto register_functor = RegisterFunc<OpticalOutputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalOutputPowerChangedEvent::GetInterval,
      &DontProcess<uint64>);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/output-power/max
void SetUpComponentsComponentOpticalChannelStateOutputPowerMax(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree,
      &OpticalTransceiverInfo::has_output_power,
      &OpticalTransceiverInfo::output_power,
      &OpticalTransceiverInfo::Power::max, &ConvertDoubleToDecimal64OrDie);

  auto register_functor = RegisterFunc<OpticalOutputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalOutputPowerChangedEvent::GetMax,
      &ConvertDoubleToDecimal64OrDie);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/output-power
// /max-time
void SetUpComponentsComponentOpticalChannelStateOutputPowerMaxTime(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree,
      &OpticalTransceiverInfo::has_output_power,
      &OpticalTransceiverInfo::output_power,
      &OpticalTransceiverInfo::Power::max_time, &DontProcess<uint64>);

  auto register_functor = RegisterFunc<OpticalOutputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalOutputPowerChangedEvent::GetMaxTime,
      &DontProcess<uint64>);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/output-power/min
void SetUpComponentsComponentOpticalChannelStateOutputPowerMin(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree,
      &OpticalTransceiverInfo::has_output_power,
      &OpticalTransceiverInfo::output_power,
      &OpticalTransceiverInfo::Power::min, &ConvertDoubleToDecimal64OrDie);

  auto register_functor = RegisterFunc<OpticalOutputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalOutputPowerChangedEvent::GetMin,
      &ConvertDoubleToDecimal64OrDie);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/output-power
// /min-time
void SetUpComponentsComponentOpticalChannelStateOutputPowerMinTime(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree,
      &OpticalTransceiverInfo::has_output_power,
      &OpticalTransceiverInfo::output_power,
      &OpticalTransceiverInfo::Power::min_time, &DontProcess<uint64>);

  auto register_functor = RegisterFunc<OpticalOutputPowerChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      module, network_interface, &OpticalOutputPowerChangedEvent::GetMinTime,
      &DontProcess<uint64>);

  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/config/target-output-power
void SetUpComponentsComponentOpticalChannelConfigTargetOutputPower(
    double initial_value, TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = [initial_value](const GnmiEvent& /*event*/,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
    ASSIGN_OR_RETURN(::gnmi::Decimal64 decimal_value,
                     ConvertDoubleToDecimal64(initial_value));
    return SendResponse(GetResponse(path, decimal_value), stream);
  };

  auto on_set_functor =
      [module, network_interface, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const ::gnmi::TypedValue* typed_value =
        static_cast<const ::gnmi::TypedValue*>(&val);
    if (!typed_value) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Not a TypedValue!";
    }
    if (typed_value->value_case() != gnmi::TypedValue::kDecimalVal) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Expects a decimal value!";
    }
    auto decimal_val = typed_value->decimal_val();
    ASSIGN_OR_RETURN(auto output_power, ConvertDecimal64ToDouble(decimal_val));

    RETURN_IF_ERROR(SetValue(module, network_interface, tree,
                             &OpticalTransceiverInfo::set_target_output_power,
                             output_power));

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (auto& optical_port :
         *new_config->mutable_optical_network_interfaces()) {
      if (optical_port.module() == module &&
          optical_port.network_interface() == network_interface) {
        optical_port.set_target_output_power(output_power);
        break;
      }
    }

    auto poll_functor = [decimal_val](const GnmiEvent& /*event*/,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
      return SendResponse(GetResponse(path, decimal_val), stream);
    };
    node->SetOnPollHandler(poll_functor)->SetOnTimerHandler(poll_functor);

    return ::util::OkStatus();
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/operational-mode
void SetUpComponentsComponentOpticalChannelStateOperationalMode(
    TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = GetOnPollFunctor(
      module, network_interface, tree,
      &OpticalTransceiverInfo::operational_mode, &DontProcess<uint64>);
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/config/operational-mode
void SetUpComponentsComponentOpticalChannelConfigOperationalMode(
    uint64 initial_value, TreeNode* node, YangParseTree* tree, int32 module,
    int32 network_interface) {
  auto poll_functor = [initial_value](const GnmiEvent& /*event*/,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, initial_value), stream);
  };
  auto on_set_functor =
      [module, network_interface, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    auto typed_value = static_cast<const gnmi::TypedValue*>(&val);
    if (!typed_value) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Not a TypedValue!";
    }
    if (typed_value->value_case() != gnmi::TypedValue::kUintVal) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Expects a uint64 value!";
    }

    uint64 uint_val = typed_value->uint_val();
    RETURN_IF_ERROR(SetValue(module, network_interface, tree,
                             &OpticalTransceiverInfo::set_operational_mode,
                             uint_val));

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (auto& optical_port :
         *new_config->mutable_optical_network_interfaces()) {
      if (optical_port.module() == module &&
          optical_port.network_interface() == network_interface) {
        optical_port.set_operational_mode(uint_val);
        break;
      }
    }

    auto poll_functor = [uint_val](const GnmiEvent& /*event*/,
                                   const ::gnmi::Path& path,
                                   GnmiSubscribeStream* stream) {
      return SendResponse(GetResponse(path, uint_val), stream);
    };
    node->SetOnPollHandler(poll_functor)->SetOnTimerHandler(poll_functor);

    return ::util::OkStatus();
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/config/line-port
void SetUpComponentsComponentOpticalChannelConfigLinePort(
    const std::string& line_port, TreeNode* node) {
  auto poll_functor = [line_port](const GnmiEvent& /*event*/,
                                  const ::gnmi::Path& path,
                                  GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, line_port), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/optical-channel/state/line-port
void SetUpComponentsComponentOpticalChannelStateLinePort(
    const std::string& line_port, TreeNode* node) {
  auto poll_functor = [line_port](const GnmiEvent& /*event*/,
                                  const ::gnmi::Path& path,
                                  GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, line_port), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

} // namespace

//////////////////////////////////////
//  AddSubtreeInterfaceFromOptical  //
//////////////////////////////////////

void YangParseTreePaths::AddSubtreeInterfaceFromOptical(
    const OpticalNetworkInterface& optical_port, YangParseTree* tree) {
  const std::string& name =
      optical_port.name().empty()
          ? absl::StrFormat("netif-%d", optical_port.network_interface())
          : optical_port.name();
  int32 module = optical_port.module();
  int32 network_interface = optical_port.network_interface();
  TreeNode* node{nullptr};

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("state")("frequency")());
  SetUpComponentsComponentOpticalChannelStateFrequency(node, tree, module,
                                                       network_interface);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("config")("frequency")());
  SetUpComponentsComponentOpticalChannelConfigFrequency(
      optical_port.frequency(), node, tree, module, network_interface);

  node = tree->AddNode(GetPath("components")("component", name)(
      "optical-channel")("state")("input-power")("instant")());
  SetUpComponentsComponentOpticalChannelStateInputPowerInstant(
      node, tree, module, network_interface);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("state")("input-power")("avg")());
  SetUpComponentsComponentOpticalChannelStateInputPowerAvg(node, tree, module,
                                                           network_interface);

  node = tree->AddNode(GetPath("components")("component", name)(
      "optical-channel")("state")("input-power")("interval")());
  SetUpComponentsComponentOpticalChannelStateInputPowerInterval(
      node, tree, module, network_interface);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("state")("input-power")("max")());
  SetUpComponentsComponentOpticalChannelStateInputPowerMax(node, tree, module,
                                                           network_interface);

  node = tree->AddNode(GetPath("components")("component", name)(
      "optical-channel")("state")("input-power")("max-time")());
  SetUpComponentsComponentOpticalChannelStateInputPowerMaxTime(
      node, tree, module, network_interface);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("state")("input-power")("min")());
  SetUpComponentsComponentOpticalChannelStateInputPowerMin(node, tree, module,
                                                           network_interface);

  node = tree->AddNode(GetPath("components")("component", name)(
      "optical-channel")("state")("input-power")("min-time")());
  SetUpComponentsComponentOpticalChannelStateInputPowerMinTime(
      node, tree, module, network_interface);

  node = tree->AddNode(GetPath("components")("component", name)(
      "optical-channel")("state")("output-power")("instant")());
  SetUpComponentsComponentOpticalChannelStateOutputPowerInstant(
      node, tree, module, network_interface);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("state")("output-power")("avg")());
  SetUpComponentsComponentOpticalChannelStateOutputPowerAvg(node, tree, module,
                                                            network_interface);

  node = tree->AddNode(GetPath("components")("component", name)(
      "optical-channel")("state")("output-power")("interval")());
  SetUpComponentsComponentOpticalChannelStateOutputPowerInterval(
      node, tree, module, network_interface);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("state")("output-power")("max")());
  SetUpComponentsComponentOpticalChannelStateOutputPowerMax(node, tree, module,
                                                            network_interface);

  node = tree->AddNode(GetPath("components")("component", name)(
      "optical-channel")("state")("output-power")("max-time")());
  SetUpComponentsComponentOpticalChannelStateOutputPowerMaxTime(
      node, tree, module, network_interface);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("state")("output-power")("min")());
  SetUpComponentsComponentOpticalChannelStateOutputPowerMin(node, tree, module,
                                                            network_interface);

  node = tree->AddNode(GetPath("components")("component", name)(
      "optical-channel")("state")("output-power")("min-time")());
  SetUpComponentsComponentOpticalChannelStateOutputPowerMinTime(
      node, tree, module, network_interface);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("config")("target-output-power")());
  SetUpComponentsComponentOpticalChannelConfigTargetOutputPower(
      optical_port.target_output_power(), node, tree, module,
      network_interface);

  // Currently, the OpenConfig considers a 16-bit uint type to represent a
  // vendor-specific bitmask for the operational-mode leaves. It might be split
  // into several independent leaves in the future.
  //
  // In Stratum, we use 64-bit value at the moment because of the absence of a
  // 16-bit uint type among the types which are supported by gNMI protocol.
  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("state")("operational-mode")());
  SetUpComponentsComponentOpticalChannelStateOperationalMode(node, tree, module,
                                                             network_interface);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("config")("operational-mode")());
  SetUpComponentsComponentOpticalChannelConfigOperationalMode(
      optical_port.operational_mode(), node, tree, module, network_interface);

  const std::string& line_port = optical_port.line_port();
  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("state")("line-port")());
  SetUpComponentsComponentOpticalChannelStateLinePort(line_port, node);

  node = tree->AddNode(GetPath("components")(
      "component", name)("optical-channel")("config")("line-port")());
  SetUpComponentsComponentOpticalChannelConfigLinePort(line_port, node);

  node = tree->AddNode(
      GetPath("components")("component", name)("config")("name")());
  SetUpComponentsComponentConfigName(name, node);

  node = tree->AddNode(GetPath("components")("component", name)("name")());
  SetUpComponentsComponentName(name, node);

  node = tree->AddNode(
      GetPath("components")("component", name)("state")("type")());
  SetUpComponentsComponentStateType("OPTICAL_CHANNEL", node);

  node = tree->AddNode(
      GetPath("components")("component", name)("state")("description")());
  SetUpComponentsComponentStateDescription(optical_port.name(), node);
}

}  // namespace hal
}  // namespace stratum
