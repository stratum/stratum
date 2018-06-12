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


#include "stratum/hal/lib/bcm/bcm_table_manager.h"

#include <string>

#include "google/protobuf/message.h"
#include "google/protobuf/repeated_field.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/bcm/constants.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/glue/net_util/integral_types.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "util/gtl/map_util.h"
#include "util/gtl/stl_util.h"

namespace stratum {
namespace hal {
namespace bcm {

namespace {

const gtl::flat_hash_set<P4MeterColor>& AllColors() {
  static auto* all_colors = new gtl::flat_hash_set<P4MeterColor>(
      {P4_METER_GREEN, P4_METER_YELLOW, P4_METER_RED});
  return *all_colors;
}

template <class MessageType>
inline void EraseReferencesFromRepeatedField(
    const gtl::flat_hash_set<const ::google::protobuf::Message*> references,
    ::google::protobuf::RepeatedPtrField<MessageType>* repeated_field) {
  repeated_field->erase(
      std::remove_if(repeated_field->begin(), repeated_field->end(),
                     [&references](const MessageType& field) {
                       return references.count(&field);
                     }),
      repeated_field->end());
}

// Fill a BcmTableEntryValue from a MappedField::Value source value. Any u32,
// u64, or b will be copied directly to the destination value. Other types are
// ignored.
void FillBcmTableEntryValue(const MappedField::Value& source,
                            BcmTableEntryValue* destination) {
  switch (source.data_case()) {
    case MappedField::Value::kU32:
      destination->set_u32(source.u32());
      break;
    case MappedField::Value::kU64:
      destination->set_u64(source.u64());
      break;
    case MappedField::Value::kB:
      destination->set_b(source.b());
      break;
    case MappedField::Value::kRawPiMatch:
      VLOG(1) << "Skipping raw match: " << source.ShortDebugString() << ".";
      break;
    case MappedField::Value::DATA_NOT_SET:
      // Don't do anything if there is no value.
      break;
  }
}

// Fill a BcmTableEntryValue from a P4ActionField source value. Any u32, u64, or
// b will be copied directly to the destination value. Other types are ignored.
void FillBcmTableEntryValue(const P4ActionFunction::P4ActionFields& source,
                            BcmTableEntryValue* destination) {
  switch (source.value_case()) {
    case P4ActionFunction::P4ActionFields::kU32:
      destination->set_u32(source.u32());
      break;
    case P4ActionFunction::P4ActionFields::kU64:
      destination->set_u64(source.u64());
      break;
    case P4ActionFunction::P4ActionFields::kB:
      destination->set_b(source.b());
      break;
    case MappedField::Value::DATA_NOT_SET:
      // Don't do anything if there is no value.
      break;
  }
}

// Fill a BcmField with data from a MappedField (from a CommonFlowEntry).
void FillBcmField(BcmField::Type type, const MappedField& source,
                  BcmField* bcm_field) {
  bcm_field->set_type(type);
  if (source.has_value()) {
    FillBcmTableEntryValue(source.value(), bcm_field->mutable_value());
  }
  if (source.has_mask()) {
    FillBcmTableEntryValue(source.mask(), bcm_field->mutable_mask());
  }
}

// Fill a simple (1-parameter) BcmAction using the provided action & parameter
// types. Parameter data is copied from the P4ActionFields source.
::util::Status FillSimpleBcmAction(
    const P4ActionFunction::P4ActionFields& source, BcmAction::Type action_type,
    BcmAction::Param::Type param_type, BcmAction* bcm_action) {
  bcm_action->set_type(action_type);
  BcmAction::Param* param = bcm_action->add_params();
  param->set_type(param_type);
  FillBcmTableEntryValue(source, param->mutable_value());
  if (param->value().data_case() == BcmTableEntryValue::DATA_NOT_SET) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "No value was found in action "
                                         << source.ShortDebugString() << ".";
  }
  return ::util::OkStatus();
}

// Returns the BcmFieldType that corresponds to the input P4FieldType.
// (CommonFlowEntry field type --> BCM field type).
BcmField::Type GetBcmFieldType(P4FieldType p4_field_type) {
  static const auto* conversion_map =
      new gtl::flat_hash_map<P4FieldType, BcmField::Type,
                             EnumHash<P4FieldType>>({
          {P4_FIELD_TYPE_UNKNOWN, BcmField::UNKNOWN},
          {P4_FIELD_TYPE_ETH_SRC, BcmField::ETH_SRC},
          {P4_FIELD_TYPE_ETH_DST, BcmField::ETH_DST},
          {P4_FIELD_TYPE_ETH_TYPE, BcmField::ETH_TYPE},
          {P4_FIELD_TYPE_VLAN_VID, BcmField::VLAN_VID},
          {P4_FIELD_TYPE_VLAN_PCP, BcmField::VLAN_PCP},
          {P4_FIELD_TYPE_IPV4_SRC, BcmField::IPV4_SRC},
          {P4_FIELD_TYPE_IPV4_DST, BcmField::IPV4_DST},
          {P4_FIELD_TYPE_IPV4_PROTO, BcmField::IP_PROTO_NEXT_HDR},
          {P4_FIELD_TYPE_IPV4_DIFFSERV, BcmField::IP_DSCP_TRAF_CLASS},
          {P4_FIELD_TYPE_NW_TTL, BcmField::IP_TTL_HOP_LIMIT},
          // TODO: Use BcmField::IPV6_SRC & BcmField::IPV6_DST if
          // prefix length > 64 bits or if this is not an ACL table. Requires a
          // refactor of this code.
          {P4_FIELD_TYPE_IPV6_SRC, BcmField::IPV6_SRC_UPPER_64},
          {P4_FIELD_TYPE_IPV6_DST, BcmField::IPV6_DST_UPPER_64},
          {P4_FIELD_TYPE_IPV6_NEXT_HDR, BcmField::IP_PROTO_NEXT_HDR},
          {P4_FIELD_TYPE_IPV6_TRAFFIC_CLASS, BcmField::IP_DSCP_TRAF_CLASS},
          {P4_FIELD_TYPE_ICMP_TYPE, BcmField::ICMP_TYPE_CODE},
          {P4_FIELD_TYPE_ICMP_CODE, BcmField::ICMP_TYPE_CODE},
          {P4_FIELD_TYPE_L4_SRC_PORT, BcmField::L4_SRC},
          {P4_FIELD_TYPE_L4_DST_PORT, BcmField::L4_DST},
          {P4_FIELD_TYPE_VRF, BcmField::VRF},
          // TODO: IFP can match on multiple class IDs, including
          // VFP and L3. P4 needs to recognize that these are different metadata
          // fields.
          {P4_FIELD_TYPE_CLASS_ID, BcmField::VFP_DST_CLASS_ID},
          {P4_FIELD_TYPE_EGRESS_PORT, BcmField::OUT_PORT},
          {P4_FIELD_TYPE_INGRESS_PORT, BcmField::IN_PORT},
          {P4_FIELD_TYPE_CLONE_PORT, BcmField::CLONE_PORT},
          // Currently unsupported field types below.
          {P4_FIELD_TYPE_ARP_TPA, BcmField::UNKNOWN},
          {P4_FIELD_TYPE_COLOR, BcmField::UNKNOWN},
          {P4_FIELD_TYPE_IN_METER, BcmField::UNKNOWN},
      });
  return gtl::FindWithDefault(*conversion_map, p4_field_type,
                              BcmField::UNKNOWN);
}

// Adds a color parameter to a BcmAction.
void AddBcmActionColorParam(P4MeterColor color, BcmAction* bcm_action) {
  BcmAction::Param* param = nullptr;
  switch (color) {
    case P4_METER_GREEN:
      param = bcm_action->add_params();
      param->set_type(BcmAction::Param::COLOR);
      param->mutable_value()->set_u32(BcmAction::Param::GREEN);
      break;
    case P4_METER_YELLOW:
      param = bcm_action->add_params();
      param->set_type(BcmAction::Param::COLOR);
      param->mutable_value()->set_u32(BcmAction::Param::YELLOW);
      break;
    case P4_METER_RED:
      param = bcm_action->add_params();
      param->set_type(BcmAction::Param::COLOR);
      param->mutable_value()->set_u32(BcmAction::Param::RED);
      break;
    default:  // Default case is colorless.
      break;
  }
}

// Returns the BCM color corresponding to the input P4MeterColor.
// (CommonFlowEntry color --> BCM color).
bool P4ColorToBcm(P4MeterColor p4_color, BcmAction::Param::Color* bcm_color) {
  static const auto* color_map =
      new gtl::flat_hash_map<P4MeterColor, BcmAction::Param::Color>{
          {P4_METER_GREEN, BcmAction::Param::GREEN},
          {P4_METER_YELLOW, BcmAction::Param::YELLOW},
          {P4_METER_RED, BcmAction::Param::RED},
      };
  const BcmAction::Param::Color* lookup = gtl::FindOrNull(*color_map, p4_color);
  if (lookup == nullptr) {
    return false;
  }
  *bcm_color = *lookup;
  return true;
}

// Creates colored BCM actions based on a template action and a given collection
// of colors. Populates a vector of BcmActions, one for each color. If no colors
// are given, the template (uncolored) action is returned.
template <class C>  // Iterable collection of P4 colors.
::util::Status FillBcmActionColorParams(
    C p4_colors, const BcmAction& bcm_action,
    std::vector<BcmAction>* output_actions) {
  // Adding all colors is the same as not specifying a color at all.
  if (p4_colors.empty() || p4_colors.size() == AllColors().size()) {
    output_actions->push_back(bcm_action);
    return ::util::OkStatus();
  }
  for (auto p4_color : p4_colors) {
    BcmAction::Param::Color bcm_color;
    if (!P4ColorToBcm(static_cast<P4MeterColor>(p4_color), &bcm_color)) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Failed to convert P4 color " << p4_color << " to Bcm color.";
    }
    BcmAction color_action = bcm_action;
    BcmAction::Param* param = color_action.add_params();
    param->set_type(BcmAction::Param::COLOR);
    param->mutable_value()->set_u32(bcm_color);
    output_actions->push_back(color_action);
  }
  return ::util::OkStatus();
}

}  // namespace

BcmTableManager::BcmTableManager(const BcmChassisManager* bcm_chassis_manager,
                                 P4TableMapper* p4_table_mapper, int unit)
    : port_id_to_logical_port_(),
      trunk_id_to_trunk_port_(),
      member_id_to_nexthop_info_(),
      group_id_to_nexthop_info_(),
      members_(),
      groups_(),
      bcm_chassis_manager_(CHECK_NOTNULL(bcm_chassis_manager)),
      p4_table_mapper_(CHECK_NOTNULL(p4_table_mapper)),
      node_id_(0),
      unit_(unit) {}

BcmTableManager::BcmTableManager()
    : port_id_to_logical_port_(),
      trunk_id_to_trunk_port_(),
      member_id_to_nexthop_info_(),
      group_id_to_nexthop_info_(),
      members_(),
      groups_(),
      bcm_chassis_manager_(nullptr),
      p4_table_mapper_(nullptr),
      node_id_(0),
      unit_(-1) {}

BcmTableManager::~BcmTableManager() {}

::util::Status BcmTableManager::PushChassisConfig(const ChassisConfig& config,
                                                  uint64 node_id) {
  node_id_ = node_id;  // Save node_id ASAP to ensure all the methods can refer
                       // to correct ID in the messages/errors.

  // Get the most updated maps from BcmChassisManager. Note that config is
  // pushed to BcmChassisManager before we get to this method (enforced via
  // testing).
  ASSIGN_OR_RETURN(const auto& port_id_to_unit_logical_port,
                   bcm_chassis_manager_->GetPortIdToUnitLogicalPortMap());
  ASSIGN_OR_RETURN(const auto& trunk_id_to_unit_trunk_port,
                   bcm_chassis_manager_->GetTrunkIdToUnitTrunkPortMap());
  for (const auto& entry : port_id_to_unit_logical_port) {
    uint64 port_id = entry.first;
    int unit = entry.second.first;
    int logical_port = entry.second.second;
    if (unit != unit_) continue;
    port_id_to_logical_port_[port_id] = logical_port;
  }
  for (const auto& entry : trunk_id_to_unit_trunk_port) {
    uint64 trunk_id = entry.first;
    int unit = entry.second.first;
    int trunk_port = entry.second.second;
    if (unit != unit_) continue;
    trunk_id_to_trunk_port_[trunk_id] = trunk_port;
  }

  // TODO: You are not done yet. You need to make sure any change in
  // the port maps (e.g. due to change in the flex ports) are reflected in the
  // programmed flows and groups as well.

  return ::util::OkStatus();
}

::util::Status BcmTableManager::VerifyChassisConfig(const ChassisConfig& config,
                                                    uint64 node_id) {
  if (node_id == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid node ID.";
  }
  if (node_id_ > 0 && node_id_ != node_id) {
    return MAKE_ERROR(ERR_REBOOT_REQUIRED)
           << "Detected a change in the node_id (" << node_id_ << " vs "
           << node_id << ").";
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::PushForwardingPipelineConfig(
    const ::p4::ForwardingPipelineConfig& config) {
  // TODO: Implement this function if needed.
  return ::util::OkStatus();
}

::util::Status BcmTableManager::VerifyForwardingPipelineConfig(
    const ::p4::ForwardingPipelineConfig& config) {
  // TODO: Implement this function if needed.
  return ::util::OkStatus();
}

::util::Status BcmTableManager::Shutdown() {
  port_id_to_logical_port_.clear();
  trunk_id_to_trunk_port_.clear();
  members_.clear();
  groups_.clear();
  gtl::STLDeleteValues(&member_id_to_nexthop_info_);
  gtl::STLDeleteValues(&group_id_to_nexthop_info_);

  return ::util::OkStatus();
}

BcmField::Type BcmTableManager::P4FieldTypeToBcmFieldType(
    P4FieldType p4_field_type) const {
  return GetBcmFieldType(p4_field_type);
}

::util::Status BcmTableManager::CommonFlowEntryToBcmFlowEntry(
    const CommonFlowEntry& common_flow_entry,
    BcmFlowEntry* bcm_flow_entry) const {
  string common_flow_entry_string = absl::StrCat(
      " CommonFlowEntry is ", common_flow_entry.ShortDebugString(), ".");
  // bcm_flow_entry.unit
  //
  // Find the unit where we will program the flow.
  bcm_flow_entry->set_unit(unit_);

  // bcm_flow_entry.bcm_table_type
  //
  // Find the BCM-specific table ID.
  ASSIGN_OR_RETURN(BcmFlowEntry::BcmTableType bcm_table_type,
                   GetBcmTableType(common_flow_entry));
  bcm_flow_entry->set_bcm_table_type(bcm_table_type);
  const AclTable* acl_table = nullptr;
  if (bcm_table_type == BcmFlowEntry::BCM_TABLE_ACL) {
    uint32 table_id = common_flow_entry.table_info().id();
    acl_table = gtl::FindOrNull(acl_tables_, table_id);
    if (acl_table == nullptr) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "TableEntry for table " << table_id
             << " was marked as ACL but table " << table_id
             << " is not an ACL table. This is a bug.";
    }
    bcm_flow_entry->set_bcm_acl_table_id(acl_table->PhysicalTableId());
    bcm_flow_entry->set_acl_stage(acl_table->Stage());
  }

  // bcm_table_entry.fields
  bool has_vrf_field = false;
  for (const auto& field : common_flow_entry.fields()) {
    // Skip fields that have no values.
    if (!field.has_value()) continue;
    auto* bcm_field = bcm_flow_entry->add_fields();
    RETURN_IF_ERROR(MappedFieldToBcmField(bcm_table_type, field, bcm_field))
        << common_flow_entry_string;
    if (field.type() == P4_FIELD_TYPE_VRF) {
      has_vrf_field = true;
    }
  }

  // Make sure for the case of L3 LPM, VRF is always set.
  if (bcm_table_type == BcmFlowEntry::BCM_TABLE_IPV4_LPM ||
      bcm_table_type == BcmFlowEntry::BCM_TABLE_IPV6_LPM) {
    CHECK_RETURN_IF_FALSE(has_vrf_field)
        << "VRF not set for an L3 LPM flow: " << common_flow_entry_string;
  }

  // bcm_table_entry.priority
  //
  // Note that it does not make sense for non-ACL flows to have priority and
  // priority will be ignored when writing to the HW. However, controller may
  // still use priority for its own reconciliation purposes.
  uint32 priority = common_flow_entry.priority();
  if (priority < 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Priority " << priority << " is negative."
           << common_flow_entry_string;
  }
  if (acl_table != nullptr) {
    if (priority >= kAclTablePriorityRange) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "ACL priority " << priority
             << " is too large. Priority must be less than "
             << kAclTablePriorityRange << "." << common_flow_entry_string;
    }
    // Add the table priority. This allows us to separate logical tables within
    // the same physical table. The priority in the CommonFlowEntry is the
    // relative priority within the table.
    priority += acl_table->Priority() * kAclTablePriorityRange;
  }
  bcm_flow_entry->set_priority(priority);

  // bcm_table_entry.actions
  //
  // Common -> BCM action mapping. Actions are typically BCM-specific.
  // At this point we are implicitly assuming ActionProfile is used for
  // ECMP/WCMP only and nothing else. Revise if needed.
  switch (common_flow_entry.action().type()) {
    case P4_ACTION_TYPE_PROFILE_MEMBER_ID: {
      uint32 member_id = common_flow_entry.action().profile_member_id();
      ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                       GetBcmNonMultipathNexthopInfo(member_id));
      auto* bcm_action = bcm_flow_entry->add_actions();
      auto* param = bcm_action->add_params();
      param->set_type(BcmAction::Param::EGRESS_INTF_ID);
      param->mutable_value()->set_u32(member_nexthop_info->egress_intf_id);
      switch (member_nexthop_info->type) {
        case BcmNonMultipathNexthop::NEXTHOP_TYPE_DROP:
          bcm_action->set_type(BcmAction::DROP);
          break;
        case BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT:
          bcm_action->set_type(BcmAction::OUTPUT_PORT);
          break;
        case BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK:
          bcm_action->set_type(BcmAction::OUTPUT_TRUNK);
          break;
        default:
          return MAKE_ERROR(ERR_INTERNAL)
                 << "Invalid or unsupported nexthop type: "
                 << BcmNonMultipathNexthop::Type_Name(member_nexthop_info->type)
                 << ", for member_id " << member_id << "."
                 << common_flow_entry_string;
      }
      break;
    }
    case P4_ACTION_TYPE_PROFILE_GROUP_ID: {
      uint32 group_id = common_flow_entry.action().profile_group_id();
      ASSIGN_OR_RETURN(BcmMultipathNexthopInfo * group_nexthop_info,
                       GetBcmMultipathNexthopInfo(group_id));
      auto* bcm_action = bcm_flow_entry->add_actions();
      bcm_action->set_type(BcmAction::OUTPUT_L3);
      auto* param = bcm_action->add_params();
      param->set_type(BcmAction::Param::EGRESS_INTF_ID);
      param->mutable_value()->set_u32(group_nexthop_info->egress_intf_id);
      break;
    }
    case P4_ACTION_TYPE_FUNCTION: {
      P4ActionFunction function = common_flow_entry.action().function();
      // Handle complex actions.
      {
        std::vector<BcmAction> bcm_actions;
        RETURN_IF_ERROR(ConvertComplexP4Actions(&function, &bcm_actions))
            << " Failed to convert CommonFlowEntry to BcmFlowEntry for unit "
            << unit_ << "." << common_flow_entry_string;
        for (const BcmAction& bcm_action : bcm_actions) {
          *bcm_flow_entry->add_actions() = bcm_action;
        }
      }
      // Handle the remaining primitive actions.
      for (const auto& primitive : function.primitives()) {
        if (primitive.op_code() == P4_ACTION_OP_DROP) {
          BcmAction drop_action;
          drop_action.set_type(BcmAction::DROP);
          std::vector<BcmAction> bcm_actions;
          RETURN_IF_ERROR(FillBcmActionColorParams(primitive.meter_colors(),
                                                   drop_action, &bcm_actions))
              << " Failed to convert CommonFlowEntry to BCM flow entry on"
              << " unit " << unit_ << "." << common_flow_entry_string;
          for (const BcmAction& bcm_action : bcm_actions) {
            *bcm_flow_entry->add_actions() = bcm_action;
          }
        }
        // No other action primitive is important at this point.
      }
      // Convert the remaining action fields to bcm fields.
      for (const auto& field : function.modify_fields()) {
        BcmAction bcm_action;
        RETURN_IF_ERROR(P4ActionFieldToBcmAction(field, &bcm_action))
            << "Failed to convert CommonFlowEntry to BCM flow entry in unit "
            << unit_ << "." << common_flow_entry_string;
        if (!ProtoEqual(bcm_action, BcmAction())) {
          *bcm_flow_entry->add_actions() = bcm_action;
        }
      }
      break;
    }
    default:
      return MAKE_ERROR(ERR_INTERNAL)
             << "Invalid or unsupported P4 action type: "
             << P4ActionType_Name(common_flow_entry.action().type())
             << common_flow_entry_string;
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::FillBcmFlowEntry(
    const ::p4::TableEntry& table_entry, ::p4::Update::Type type,
    BcmFlowEntry* bcm_flow_entry) const {
  std::string error_message =
      " TableEntry is " + table_entry.ShortDebugString() + ".";

  CHECK_RETURN_IF_FALSE(table_entry.table_id())
      << "Must specify table_id for each TableEntry." << error_message;
  // Fill the CommonFlowEntry by calling P4TableMapper::MapFlowEntry(). This
  // will include all the mappings that are common to all the platforms.
  CommonFlowEntry common_flow_entry;
  RETURN_IF_ERROR(
      p4_table_mapper_->MapFlowEntry(table_entry, type, &common_flow_entry))
      << error_message;
  RETURN_IF_ERROR(
      CommonFlowEntryToBcmFlowEntry(common_flow_entry, bcm_flow_entry))
      << error_message;

  // We do not support initializing flow packet counter values.
  CHECK_RETURN_IF_FALSE(!table_entry.has_counter_data())
      << "Unsupported counter initialization given in TableEntry."
      << error_message;

  // Transfer meter configuration.
  if (table_entry.has_meter_config()) {
    // Meters are only available for ACL flows.
    if (bcm_flow_entry->bcm_table_type() != BcmFlowEntry::BCM_TABLE_ACL) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Metering is only supported for ACL flows." << error_message;
    }
    RETURN_IF_ERROR(FillBcmMeterConfig(table_entry.meter_config(),
                                       bcm_flow_entry->mutable_meter()))
        << error_message;
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::FillBcmMeterConfig(
    const ::p4::MeterConfig& p4_meter, BcmMeterConfig* bcm_meter) const {
  // Ensure that meter configuration values can be casted safely to uint32.
  if ((p4_meter.cir() < 0) || (p4_meter.cburst() < 0) ||
      (p4_meter.cir() >= 0xffffffffLL) || (p4_meter.cburst() >= 0xffffffffLL) ||
      (p4_meter.pir() < 0) || (p4_meter.pburst() < 0) ||
      (p4_meter.pir() >= 0xffffffffLL) || (p4_meter.pburst() >= 0xffffffffLL)) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Meter configuration values out of range supported by Broadcom "
           << "chip.";
  }
  // Copy the meter configuration to BcmMeterConfig.
  bcm_meter->set_committed_rate(static_cast<uint32>(p4_meter.cir()));
  bcm_meter->set_committed_burst(static_cast<uint32>(p4_meter.cburst()));
  bcm_meter->set_peak_rate(static_cast<uint32>(p4_meter.pir()));
  bcm_meter->set_peak_burst(static_cast<uint32>(p4_meter.pburst()));
  return ::util::OkStatus();
}

namespace {

// Helper to check the validity of the nexthop parameters.
::util::Status CheckBcmNonMultiPathNexthop(
    const BcmNonMultipathNexthop& nexthop) {
  if (nexthop.type() == BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT) {
    if (nexthop.logical_port() == 0 && nexthop.src_mac() == 0 &&
        nexthop.src_mac() == 0) {
      VLOG(1) << "Detected a trap to CPU nexthop: "
              << nexthop.ShortDebugString() << ".";
    } else if (nexthop.logical_port() == 0 && nexthop.src_mac() > 0 &&
               nexthop.dst_mac() > 0) {
      VLOG(1) << "Detected a nexthop to CPU with regular L3 routing: "
              << nexthop.ShortDebugString() << ".";

    } else if (nexthop.logical_port() > 0 && nexthop.src_mac() > 0 &&
               nexthop.dst_mac() > 0) {
      VLOG(1) << "Detected a nexthop to port with regular L3 routing: "
              << nexthop.ShortDebugString() << ".";

    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Detected invalid port nexthop: " << nexthop.ShortDebugString()
             << ".";
    }
  } else if (nexthop.type() == BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK) {
    if (nexthop.trunk_port() > 0 && nexthop.src_mac() > 0 &&
        nexthop.dst_mac() > 0) {
      VLOG(1) << "Detected a nexthop to trunk with regular L3 routing: "
              << nexthop.ShortDebugString() << ".";
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Detected invalid trunk nexthop: " << nexthop.ShortDebugString()
             << ".";
    }
  } else if (nexthop.type() == BcmNonMultipathNexthop::NEXTHOP_TYPE_DROP) {
    if (nexthop.logical_port() == 0 && nexthop.src_mac() == 0 &&
        nexthop.dst_mac() == 0) {
      VLOG(1) << "Detected a drop nexthop: " << nexthop.ShortDebugString()
              << ".";
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Detected invalid drop nexthop: " << nexthop.ShortDebugString()
             << ".";
    }
  } else {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Detected unknown non-multipath nexthop: "
           << nexthop.ShortDebugString() << ".";
  }

  return ::util::OkStatus();
}

}  // namespace

::util::Status BcmTableManager::FillBcmNonMultipathNexthop(
    const ::p4::ActionProfileMember& action_profile_member,
    BcmNonMultipathNexthop* bcm_non_multipath_nexthop) const {
  bcm_non_multipath_nexthop->set_unit(unit_);

  // Fill the MappedAction by calling P4TableMapper::MapActionProfile(). This
  // will include all the mappings that are common to all the platforms.
  MappedAction mapped_action;
  RETURN_IF_ERROR(p4_table_mapper_->MapActionProfileMember(
      action_profile_member, &mapped_action));

  // Common action -> BCM non-multipath nexthop mapping. If the given action
  // profile member ends up being a type we dont expect (i.e. not a nexthop),
  // we will either not find a correct type for it or the verification at the
  // end of this function will fail.
  switch (mapped_action.type()) {
    case P4_ACTION_TYPE_FUNCTION: {
      const auto& function = mapped_action.function();
      if (function.primitives_size() == 0) {
        // We have the following cases for egress:
        // 1- If CPU port is given and src_mac and dst_mac are both zero,
        //    we trap the packets to CPU. This means we skip the entire egress
        //    modification and send the packet to CPU with no change.
        // 2- If CPU port is given and src_mac and dst_mac are both non-zero, we
        //    have a case similar to any other egress intf creation with regular
        //    ports/trunks. Note that the controller needs to be very careful
        //    here as we cannot do rate limiting for such packets.
        // 3- If a regular port/trunk is given the src_mac and dst_mac both
        //    should be non-zero.
        for (const auto& field : function.modify_fields()) {
          switch (field.type()) {
            case P4_FIELD_TYPE_ETH_SRC:
              bcm_non_multipath_nexthop->set_src_mac(field.u64());
              break;
            case P4_FIELD_TYPE_ETH_DST:
              bcm_non_multipath_nexthop->set_dst_mac(field.u64());
              break;
            case P4_FIELD_TYPE_EGRESS_PORT: {
              uint64 id = static_cast<uint64>(field.u32());
              if (id == kCpuPortId) {
                // CPU port is a special case.
                bcm_non_multipath_nexthop->set_type(
                    BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT);
                bcm_non_multipath_nexthop->set_logical_port(0);
              } else if (port_id_to_logical_port_.count(id)) {
                // Regular ports.
                bcm_non_multipath_nexthop->set_type(
                    BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT);
                bcm_non_multipath_nexthop->set_logical_port(
                    port_id_to_logical_port_.at(id));
              } else if (trunk_id_to_trunk_port_.count(id)) {
                // Trunk/LAG ports.
                bcm_non_multipath_nexthop->set_type(
                    BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK);
                bcm_non_multipath_nexthop->set_trunk_port(
                    trunk_id_to_trunk_port_.at(id));
              } else {
                return MAKE_ERROR(ERR_INVALID_PARAM)
                       << "Could not find logical port or trunk port for port "
                       << "id " << id << " on unit " << unit_ << ".";
              }
              break;
            }
            default:
              return MAKE_ERROR(ERR_INVALID_PARAM)
                     << "Invalid or unsupported P4 field type to modify: "
                     << P4FieldType_Name(field.type()) << ". MappedAction is "
                     << mapped_action.ShortDebugString()
                     << ". ActionProfileMember is "
                     << action_profile_member.ShortDebugString() << ".";
          }
        }
      } else if (function.primitives_size() == 1 &&
                 function.primitives(0).op_code() == P4_ACTION_OP_DROP) {
        bcm_non_multipath_nexthop->set_type(
            BcmNonMultipathNexthop::NEXTHOP_TYPE_DROP);
        bcm_non_multipath_nexthop->set_src_mac(0);
        bcm_non_multipath_nexthop->set_dst_mac(0);
        bcm_non_multipath_nexthop->set_logical_port(0);
      } else {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid action premitives, found in "
               << mapped_action.ShortDebugString()
               << ". ActionProfileMember is "
               << action_profile_member.ShortDebugString() << ".";
      }
      break;
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid or unsupported P4 mapped action type: "
             << P4ActionType_Name(mapped_action.type()) << ", found in "
             << mapped_action.ShortDebugString() << ". ActionProfileMember is "
             << action_profile_member.ShortDebugString() << ".";
  }

  // Now we need to make sure the bcm_non_multipath_nexthop is OK.
  return CheckBcmNonMultiPathNexthop(*bcm_non_multipath_nexthop);
}

::util::Status BcmTableManager::FillBcmMultipathNexthop(
    const ::p4::ActionProfileGroup& action_profile_group,
    BcmMultipathNexthop* bcm_multipath_nexthop) const {
  bcm_multipath_nexthop->set_unit(unit_);

  // Fill the MappedAction by calling P4TableMapper::MapActionProfile(). For
  // the case of ECMP/WCMP group, this function only checks the for the
  // validity of the action profile.
  MappedAction unused_mapped_action;
  RETURN_IF_ERROR(p4_table_mapper_->MapActionProfileGroup(
      action_profile_group, &unused_mapped_action));

  // Action profile entry -> BCM multipath nexthop mapping.
  for (const auto& member : action_profile_group.members()) {
    uint32 member_id = member.member_id();
    uint32 weight = std::max(member.weight(), 1);
    ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                     GetBcmNonMultipathNexthopInfo(member_id));
    auto* nexthop_member = bcm_multipath_nexthop->add_members();
    nexthop_member->set_egress_intf_id(member_nexthop_info->egress_intf_id);
    nexthop_member->set_weight(weight);
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::AddTableEntry(
    const ::p4::TableEntry& table_entry) {
  uint32 table_id = table_entry.table_id();
  if (table_id == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Cannot insert flow with table id 0: "
           << table_entry.ShortDebugString() << ".";
  }
  util::StatusOr<BcmFlowTable*> result = GetMutableFlowTable(table_id);
  if (result.ok()) {
    RETURN_IF_ERROR(result.ValueOrDie()->InsertEntry(table_entry));
  } else {
    auto table_result =
        generic_flow_tables_.emplace(std::make_pair(table_id, table_id));
    if (!table_result.second) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to add new table with id " << table_id << ".";
    }
    RETURN_IF_ERROR(table_result.first->second.InsertEntry(table_entry));
  }

  // Update the flow_ref_count for the member or group.
  uint32 member_id = table_entry.action().action_profile_member_id();
  uint32 group_id = table_entry.action().action_profile_group_id();
  if (member_id > 0) {
    RETURN_IF_ERROR(UpdateFlowRefCountForMember(member_id, 1));
  } else if (group_id > 0) {
    RETURN_IF_ERROR(UpdateFlowRefCountForGroup(group_id, 1));
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::UpdateTableEntry(
    const ::p4::TableEntry& table_entry) {
  uint32 table_id = table_entry.table_id();
  ASSIGN_OR_RETURN(BcmFlowTable * table, GetMutableFlowTable(table_id),
                   _ << "Could not find table " << table_id << ".");
  ASSIGN_OR_RETURN(
      ::p4::TableEntry old_entry, table->ModifyEntry(table_entry),
      _ << "Failed to update flow " << table_entry.ShortDebugString() << ".");

  // Update the flow_ref_count for the old/new member or group.
  uint32 old_member_id = old_entry.action().action_profile_member_id();
  uint32 old_group_id = old_entry.action().action_profile_group_id();
  uint32 new_member_id = table_entry.action().action_profile_member_id();
  uint32 new_group_id = table_entry.action().action_profile_group_id();
  if (old_member_id > 0 && old_member_id == new_member_id) {
    // Nothing to do in this case. Old and new flows point to the same member.
    return ::util::OkStatus();
  }
  if (old_group_id > 0 && old_group_id == new_group_id) {
    // Nothing to do in this case. Old and new flows point to the same group.
    return ::util::OkStatus();
  }
  if (old_member_id > 0) {
    RETURN_IF_ERROR(UpdateFlowRefCountForMember(old_member_id, -1));
  } else if (old_group_id > 0) {
    RETURN_IF_ERROR(UpdateFlowRefCountForGroup(old_group_id, -1));
  }
  if (new_member_id > 0) {
    RETURN_IF_ERROR(UpdateFlowRefCountForMember(new_member_id, 1));
  } else if (new_group_id > 0) {
    RETURN_IF_ERROR(UpdateFlowRefCountForGroup(new_group_id, 1));
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::DeleteTableEntry(
    const ::p4::TableEntry& table_entry) {
  uint32 table_id = table_entry.table_id();
  ASSIGN_OR_RETURN(BcmFlowTable* table, GetMutableFlowTable(table_id),
                   _ << "Could not find table " << table_id << ".");
  ASSIGN_OR_RETURN(
      ::p4::TableEntry old_entry, table->DeleteEntry(table_entry),
      _ << "Failed to delete flow " << table_entry.ShortDebugString() << ".");

  // Update the flow_ref_count for the member or group.
  uint32 member_id = old_entry.action().action_profile_member_id();
  uint32 group_id = old_entry.action().action_profile_group_id();
  if (member_id > 0) {
    RETURN_IF_ERROR(UpdateFlowRefCountForMember(member_id, -1));
  } else if (group_id > 0) {
    RETURN_IF_ERROR(UpdateFlowRefCountForGroup(group_id, -1));
  }

  // If this is the last entry in a generic table, remove the generic table.
  auto table_iter = generic_flow_tables_.find(table_id);
  if (table_iter != generic_flow_tables_.end() && table_iter->second.Empty()) {
    generic_flow_tables_.erase(table_iter);
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::UpdateTableEntryMeter(
    const ::p4::DirectMeterEntry& meter) {
  const ::p4::TableEntry& table_entry = meter.table_entry();
  uint32 table_id = table_entry.table_id();
  // Only ACL flows support meters.
  if (acl_tables_.count(table_id) == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Attempting to update meter configuration for non-ACL table "
              "entry: "
           << table_entry.ShortDebugString() << ".";
  }

  ASSIGN_OR_RETURN(BcmFlowTable* table, GetMutableFlowTable(table_id),
                   _ << "Could not find table " << table_id << ".");
  ASSIGN_OR_RETURN(
      ::p4::TableEntry modified_entry, table->Lookup(table_entry),
      _ << "Failed to find flow " << table_entry.ShortDebugString() << ".");
  *modified_entry.mutable_meter_config() = meter.config();
  RETURN_IF_ERROR(table->ModifyEntry(modified_entry).status())
      << "Failed to insert entry with modified meter. Entry: "
      << table_entry.ShortDebugString();
  return ::util::OkStatus();
}

::util::Status BcmTableManager::AddActionProfileMember(
    const ::p4::ActionProfileMember& action_profile_member,
    BcmNonMultipathNexthop::Type type, int egress_intf_id) {
  // Sanity checking.
  if (!action_profile_member.member_id() ||
      !action_profile_member.action_profile_id()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Need non-zero member_id and action_profile_id: "
           << action_profile_member.ShortDebugString() << ".";
  }
  uint32 member_id = action_profile_member.member_id();

  // The egress intf ID for this member must not be assigned to an existing
  // member.
  using Entry = std::pair<uint32, BcmNonMultipathNexthopInfo*>;
  auto it = std::find_if(member_id_to_nexthop_info_.begin(),
                         member_id_to_nexthop_info_.end(),
                         [egress_intf_id](const Entry& e) {
                           return e.second->egress_intf_id == egress_intf_id;
                         });
  if (it != member_id_to_nexthop_info_.end()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Member with ID " << member_id << " is supposed to point to "
           << "egress intf with ID " << egress_intf_id << ". However this "
           << "egress intf is already assigned to member with ID " << it->first
           << ".";
  }

  // Add an BcmNonMultipathNexthopInfo for the member.
  auto* member_nexthop_info = new BcmNonMultipathNexthopInfo();
  member_nexthop_info->egress_intf_id = egress_intf_id;
  member_nexthop_info->type = type;
  if (!gtl::InsertIfNotPresent(&member_id_to_nexthop_info_, member_id,
                               member_nexthop_info)) {
    delete member_nexthop_info;
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Cannot add already existing member_id: " << member_id << ".";
  }

  // Save a copy of ::p4::ActionProfileMember.
  if (!gtl::InsertIfNotPresent(&members_, action_profile_member)) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Inconsistent state. Member with ID " << member_id << " already "
           << "exists in members_.";
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::AddActionProfileGroup(
    const ::p4::ActionProfileGroup& action_profile_group, int egress_intf_id) {
  // Sanity checking.
  if (!action_profile_group.group_id() ||
      !action_profile_group.action_profile_id()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Need non-zero group_id and action_profile_id: "
           << action_profile_group.ShortDebugString() << ".";
  }
  uint32 group_id = action_profile_group.group_id();

  // Group must not exist when calling this function (however the members of
  // the group must).
  CHECK_RETURN_IF_FALSE(!group_id_to_nexthop_info_.count(group_id))
      << "Cannot add already existing group_id: " << group_id << ".";

  // The egress intf ID for this group must not be assigned to an existing
  // group.
  using Entry = std::pair<uint32, BcmMultipathNexthopInfo*>;
  auto it = std::find_if(group_id_to_nexthop_info_.begin(),
                         group_id_to_nexthop_info_.end(),
                         [egress_intf_id](const Entry& e) {
                           return e.second->egress_intf_id == egress_intf_id;
                         });
  CHECK_RETURN_IF_FALSE(it == group_id_to_nexthop_info_.end())
      << "Group with ID " << group_id
      << " is supposed to point to egress intf with ID " << egress_intf_id
      << ". However this egress intf is already assigned to group with ID "
      << it->first << " on this node.";

  // Add an BcmMultipathNexthopInfo for the group.
  auto* group_nexthop_info = new BcmMultipathNexthopInfo();
  group_nexthop_info->egress_intf_id = egress_intf_id;
  for (const auto& member : action_profile_group.members()) {
    uint32 member_id = member.member_id();
    uint32 weight = std::max(member.weight(), 1);
    ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                     GetBcmNonMultipathNexthopInfo(member_id));
    group_nexthop_info->member_id_to_weight[member_id] = weight;
    member_nexthop_info->group_ref_count++;
  }
  if (!gtl::InsertIfNotPresent(&group_id_to_nexthop_info_, group_id,
                               group_nexthop_info)) {
    delete group_nexthop_info;
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Cannot add already existing group_id: " << group_id << ".";
  }

  // Save a copy of ::p4::ActionProfileGroup.
  if (!gtl::InsertIfNotPresent(&groups_, action_profile_group)) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Inconsistent state. Group with ID " << group_id << " already "
           << "exists in groups_.";
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::UpdateActionProfileMember(
    const ::p4::ActionProfileMember& action_profile_member,
    BcmNonMultipathNexthop::Type type) {
  uint32 member_id = action_profile_member.member_id();

  // Member must exist when calling this function. Find the corresponding
  // BcmNonMultipathNexthopInfo and update it. At the moment only type can
  // change.
  ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                   GetBcmNonMultipathNexthopInfo(member_id));
  member_nexthop_info->type = type;

  // Update the copy of ::p4::ActionProfileMember matching the input (remove the
  // old match and add the new one instead).
  CHECK_RETURN_IF_FALSE(members_.erase(action_profile_member) == 1)
      << "Inconsistent state. Old member with ID " << member_id << " did not "
      << "exist in members_.";
  members_.insert(action_profile_member);

  return ::util::OkStatus();
}

::util::Status BcmTableManager::UpdateActionProfileGroup(
    const ::p4::ActionProfileGroup& action_profile_group) {
  uint32 group_id = action_profile_group.group_id();

  // The group and all the members to add and remove to the group must exist
  // when calling this function. Find the corresponding BcmMultipathNexthopInfo
  // for the group and update it.
  ASSIGN_OR_RETURN(BcmMultipathNexthopInfo * group_nexthop_info,
                   GetBcmMultipathNexthopInfo(group_id));

  // Save a copy of old member_id_to_weight and then populate it with the new
  // members.
  auto old_member_id_to_weight = group_nexthop_info->member_id_to_weight;
  group_nexthop_info->member_id_to_weight.clear();
  for (const auto& member : action_profile_group.members()) {
    uint32 member_id = member.member_id();
    uint32 weight = std::max(member.weight(), 1);
    ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                     GetBcmNonMultipathNexthopInfo(member_id));
    group_nexthop_info->member_id_to_weight[member_id] = weight;
    if (!old_member_id_to_weight.count(member_id)) {
      // Only increase the ref count for the members which are newly added.
      member_nexthop_info->group_ref_count++;
    }
  }

  for (const auto& e : old_member_id_to_weight) {
    uint32 member_id = e.first;
    ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                     GetBcmNonMultipathNexthopInfo(member_id));
    CHECK_RETURN_IF_FALSE(member_nexthop_info->group_ref_count > 0)
        << "Non-positive group_ref_count for following member_id: " << member_id
        << ".";
    if (!group_nexthop_info->member_id_to_weight.count(member_id)) {
      // Only decrease the ref count for the members which are newly removed.
      member_nexthop_info->group_ref_count--;
    }
  }

  // Update the copy of ::p4::ActionProfileGroup matching the input (remove the
  // old match and add the new one instead).
  CHECK_RETURN_IF_FALSE(groups_.erase(action_profile_group) == 1)
      << "Inconsistent state. Old group with ID " << group_id << " did not "
      << "exist in groups_.";
  groups_.insert(action_profile_group);

  return ::util::OkStatus();
}

::util::Status BcmTableManager::DeleteActionProfileMember(
    const ::p4::ActionProfileMember& action_profile_member) {
  uint32 member_id = action_profile_member.member_id();

  // Member must exist when calling this function. Find the corresponding
  // BcmNonMultipathNexthopInfo and remove it.
  ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                   GetBcmNonMultipathNexthopInfo(member_id));
  CHECK_RETURN_IF_FALSE(member_nexthop_info->flow_ref_count == 0);
  CHECK_RETURN_IF_FALSE(member_nexthop_info->group_ref_count == 0);
  delete member_nexthop_info;
  member_id_to_nexthop_info_.erase(member_id);

  // Delete the copy of ::p4::ActionProfileMember matching the input.
  CHECK_RETURN_IF_FALSE(members_.erase(action_profile_member) == 1)
      << "Inconsistent state. Old member with ID " << member_id << " did not "
      << "exist in members_.";

  return ::util::OkStatus();
}

::util::Status BcmTableManager::DeleteActionProfileGroup(
    const ::p4::ActionProfileGroup& action_profile_group) {
  uint32 group_id = action_profile_group.group_id();

  // group and all its members must exist when calling this function. Find the
  // corresponding BcmMultipathNexthopInfo and update it.
  ASSIGN_OR_RETURN(BcmMultipathNexthopInfo * group_nexthop_info,
                   GetBcmMultipathNexthopInfo(group_id));
  for (const auto& e : group_nexthop_info->member_id_to_weight) {
    ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                     GetBcmNonMultipathNexthopInfo(e.first));
    CHECK_RETURN_IF_FALSE(member_nexthop_info->group_ref_count > 0)
        << "Non-positive group_ref_count for following member_id: " << e.first
        << ".";
    member_nexthop_info->group_ref_count--;
  }
  delete group_nexthop_info;
  group_id_to_nexthop_info_.erase(group_id);

  // Delete the copy of ::p4::ActionProfileGroup matching the input.
  CHECK_RETURN_IF_FALSE(groups_.erase(action_profile_group) == 1)
      << "Inconsistent state. Old group with ID " << group_id << " did not "
      << "exist in groups_.";

  return ::util::OkStatus();
}

::util::StatusOr<std::set<uint32>> BcmTableManager::GetGroupsForMember(
    uint32 member_id) const {
  std::set<uint32> group_ids = {};
  // TODO: Implement this.
  return group_ids;
}

bool BcmTableManager::ActionProfileMemberExists(uint32 member_id) const {
  return member_id_to_nexthop_info_.count(member_id);
}

bool BcmTableManager::ActionProfileGroupExists(uint32 group_id) const {
  return group_id_to_nexthop_info_.count(group_id);
}

::util::Status BcmTableManager::GetBcmNonMultipathNexthopInfo(
    uint32 member_id, BcmNonMultipathNexthopInfo* info) const {
  if (info == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null info.";
  }

  ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                   GetBcmNonMultipathNexthopInfo(member_id));
  info->egress_intf_id = member_nexthop_info->egress_intf_id;
  info->type = member_nexthop_info->type;
  info->group_ref_count = member_nexthop_info->group_ref_count;
  info->flow_ref_count = member_nexthop_info->flow_ref_count;

  return ::util::OkStatus();
}

::util::Status BcmTableManager::GetBcmMultipathNexthopInfo(
    uint32 group_id, BcmMultipathNexthopInfo* info) const {
  if (info == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null info.";
  }

  ASSIGN_OR_RETURN(BcmMultipathNexthopInfo * group_nexthop_info,
                   GetBcmMultipathNexthopInfo(group_id));
  info->egress_intf_id = group_nexthop_info->egress_intf_id;
  info->flow_ref_count = group_nexthop_info->flow_ref_count;
  info->member_id_to_weight = group_nexthop_info->member_id_to_weight;

  return ::util::OkStatus();
}

::util::Status BcmTableManager::AddAclTable(AclTable table) {
  if (HasTable(table.Id())) {
    return MAKE_ERROR(ERR_ENTRY_EXISTS)
           << "Cannot insert table with existing id: " << table.Id();
  }
  acl_tables_.emplace(table.Id(), std::move(table));
  return util::OkStatus();
}

::util::StatusOr<const AclTable*> BcmTableManager::GetReadOnlyAclTable(
    uint32 table_id) const {
  const AclTable* table = gtl::FindOrNull(acl_tables_, table_id);
  if (table == nullptr) {
    if (!HasTable(table_id)) {
      return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
             << "Table " << table_id << " does not exist.";
    }
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Table " << table_id << " is not an ACL table.";
  }
  return table;
}

::util::Status BcmTableManager::AddAclTableEntry(
    const ::p4::TableEntry& table_entry, int bcm_flow_id) {
  uint32 table_id = table_entry.table_id();
  AclTable* table = gtl::FindOrNull(acl_tables_, table_id);
  if (table == nullptr) {
    if (!HasTable(table_id)) {
      return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
             << "Table " << table_id << " does not exist.";
    }
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Table " << table_id << " is not an ACL table.";
  }
  RETURN_IF_ERROR(AddTableEntry(table_entry));
  RETURN_IF_ERROR(table->SetBcmAclId(table_entry, bcm_flow_id));
  return util::OkStatus();
}

std::set<uint32> BcmTableManager::GetAllAclTableIDs() const {
  std::set<uint32> ids;
  for (const auto& pair : acl_tables_) {
    ids.insert(pair.first);
  }
  return ids;
}

::util::Status BcmTableManager::DeleteTable(uint32 table_id) {
  const BcmFlowTable* table;
  ASSIGN_OR_RETURN(table, GetConstantFlowTable(table_id),
                   _ << "Could not find table " << table_id << " to delete.");
  std::vector<p4::TableEntry> entries;
  for (const auto& entry : *table) {
    entries.emplace_back(entry);
  }
  for (const auto& entry : entries) {
    ::util::Status status = DeleteTableEntry(entry);
    if (!status.ok()) {
      // If this error triggers, there is a bug.
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed while clearing entries from table " << table_id
             << ". The software state is inconsistent. Deletion error report: "
             << status;
    }
  }
  // Remove the ACL table since it is not automatically deleted when the entries
  // are removed like generic tables.
  acl_tables_.erase(table_id);
  return util::OkStatus();
}

::util::Status BcmTableManager::ReadTableEntries(
    const std::set<uint32>& table_ids, ::p4::ReadResponse* resp,
    std::vector<::p4::TableEntry*>* acl_flows) const {
  if (resp == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null resp.";
  }
  if (acl_flows == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null acl_flows.";
  }

  // Return all tables if no table ids were specified.
  if (table_ids.empty()) {
    for (const auto& pair : generic_flow_tables_) {
      for (const auto& table_entry : pair.second) {
        *resp->add_entities()->mutable_table_entry() = table_entry;
      }
    }
    // Acl entries should also be recorded in acl_flows. These are pointers to
    // the acl entries in resp.
    for (const auto& pair : acl_tables_) {
      for (const auto& table_entry : pair.second) {
        auto entry_ptr = resp->add_entities()->mutable_table_entry();
        *entry_ptr = table_entry;
        acl_flows->push_back(entry_ptr);
      }
    }
  } else {
    // Lookup each provided table id.
    for (uint32 table_id : table_ids) {
      // Lookup from the ACL tables.
      const AclTable* acl_lookup = gtl::FindOrNull(acl_tables_, table_id);
      if (acl_lookup) {
        // Acl entries should also be recorded in acl_flows. These are pointers
        // to the acl entries in resp.
        for (const auto& table_entry : *acl_lookup) {
          auto entry_ptr = resp->add_entities()->mutable_table_entry();
          *entry_ptr = table_entry;
          acl_flows->push_back(entry_ptr);
        }
        continue;
      }
      // Lookup from the generic tables.
      const BcmFlowTable* lookup =
          gtl::FindOrNull(generic_flow_tables_, table_id);
      if (lookup) {
        for (const auto& table_entry : *lookup) {
          *resp->add_entities()->mutable_table_entry() = table_entry;
        }
      }
    }
  }

  return ::util::OkStatus();
}

util::StatusOr<p4::TableEntry> BcmTableManager::LookupTableEntry(
    const ::p4::TableEntry& entry) const {
  ASSIGN_OR_RETURN(const BcmFlowTable* table,
                   GetConstantFlowTable(entry.table_id()),
                   _ << "Could not find table " << entry.table_id() << ".");
  ASSIGN_OR_RETURN(::p4::TableEntry lookup, table->Lookup(entry),
                   _ << "Table " << entry.table_id()
                     << " does not contain a matching flow for "
                     << entry.ShortDebugString() << ".");

  return lookup;
}

::util::Status BcmTableManager::ReadActionProfileMembers(
    const std::set<uint32>& action_profile_ids,
    WriterInterface<::p4::ReadResponse>* writer) const {
  if (writer == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null writer.";
  }

  ::p4::ReadResponse resp;
  for (const auto& member : members_) {
    if (action_profile_ids.empty() ||
        action_profile_ids.count(member.action_profile_id())) {
      auto* entity = resp.add_entities();
      *entity->mutable_action_profile_member() = member;
    }
  }
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::ReadActionProfileGroups(
    const std::set<uint32>& action_profile_ids,
    WriterInterface<::p4::ReadResponse>* writer) const {
  if (writer == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null writer.";
  }

  ::p4::ReadResponse resp;
  for (const auto& group : groups_) {
    if (action_profile_ids.empty() ||
        action_profile_ids.count(group.action_profile_id())) {
      auto* entity = resp.add_entities();
      *entity->mutable_action_profile_group() = group;
    }
  }
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
  }

  return ::util::OkStatus();
}

::util::Status BcmTableManager::MapFlowEntry(
    const ::p4::TableEntry& table_entry, ::p4::Update::Type type,
    CommonFlowEntry* flow_entry) const {
  return p4_table_mapper_->MapFlowEntry(table_entry, type, flow_entry);
}

std::unique_ptr<BcmTableManager> BcmTableManager::CreateInstance(
    const BcmChassisManager* bcm_chassis_manager,
    P4TableMapper* p4_table_mapper, int unit) {
  return absl::WrapUnique(
      new BcmTableManager(bcm_chassis_manager, p4_table_mapper, unit));
}

::util::Status BcmTableManager::UpdateFlowRefCountForMember(uint32 member_id,
                                                            int delta) {
  ASSIGN_OR_RETURN(BcmNonMultipathNexthopInfo * member_nexthop_info,
                   GetBcmNonMultipathNexthopInfo(member_id));
  if (delta < 0) {
    CHECK_RETURN_IF_FALSE(member_nexthop_info->flow_ref_count + delta >= 0)
        << "Not big enough flow_ref_count for following member_id: "
        << member_id
        << ". flow_ref_count = " << member_nexthop_info->flow_ref_count
        << ", delta = " << delta << ".";
  }
  member_nexthop_info->flow_ref_count += delta;

  return ::util::OkStatus();
}

::util::Status BcmTableManager::UpdateFlowRefCountForGroup(uint32 group_id,
                                                           int delta) {
  ASSIGN_OR_RETURN(BcmMultipathNexthopInfo * group_nexthop_info,
                   GetBcmMultipathNexthopInfo(group_id));
  if (delta < 0) {
    CHECK_RETURN_IF_FALSE(group_nexthop_info->flow_ref_count + delta >= 0)
        << "Not big enough flow_ref_count for following group_id: " << group_id
        << ". flow_ref_count = " << group_nexthop_info->flow_ref_count
        << ", delta = " << delta << ".";
  }
  group_nexthop_info->flow_ref_count += delta;

  return ::util::OkStatus();
}

::util::StatusOr<BcmNonMultipathNexthopInfo*>
BcmTableManager::GetBcmNonMultipathNexthopInfo(uint32 member_id) const {
  BcmNonMultipathNexthopInfo* info =
      gtl::FindPtrOrNull(member_id_to_nexthop_info_, member_id);
  if (info == nullptr) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "Unknown member_id: " << member_id << ".";
  }

  return info;
}

::util::StatusOr<BcmMultipathNexthopInfo*>
BcmTableManager::GetBcmMultipathNexthopInfo(uint32 group_id) const {
  BcmMultipathNexthopInfo* info =
      gtl::FindPtrOrNull(group_id_to_nexthop_info_, group_id);
  if (info == nullptr) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "Unknown group_id: " << group_id << ".";
  }

  return info;
}

::util::Status BcmTableManager::CreateEgressPortAction(
    uint64 port_id, BcmAction* bcm_action) const {
  // Drop dataplane packets if the destination is the CPU.
  if (port_id == kCpuPortId) {
    bcm_action->set_type(BcmAction::DROP);
    return ::util::OkStatus();
  }
  const int* port = gtl::FindOrNull(port_id_to_logical_port_, port_id);
  bool is_trunk = false;
  if (port == nullptr) {
    port = gtl::FindOrNull(trunk_id_to_trunk_port_, port_id);
    is_trunk = (port != nullptr);
  }
  if (port == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Unknown port_id " << port_id << " on node " << node_id_ << ".";
  }
  BcmAction::Param* param = nullptr;
  if (is_trunk) {
    bcm_action->set_type(BcmAction::OUTPUT_TRUNK);
    param = bcm_action->add_params();
    param->set_type(BcmAction::Param::TRUNK_PORT);
  } else {
    bcm_action->set_type(BcmAction::OUTPUT_PORT);
    param = bcm_action->add_params();
    param->set_type(BcmAction::Param::LOGICAL_PORT);
  }
  param->mutable_value()->set_u32(*port);

  return ::util::OkStatus();
}

// Convert a CommonFlowEntry.fields (MappedField) value to a BcmField.
::util::Status BcmTableManager::MappedFieldToBcmField(
    BcmFlowEntry::BcmTableType bcm_table_type, const MappedField& common_field,
    BcmField* bcm_field) const {
  // Common -> BCM flow mapping. Some of the mappings are the same for BCM,
  // however there are cases where there are differences.
  if (common_field.type() == P4_FIELD_TYPE_VRF) {
    // To prevent conversion problems when converting uint32 to int32,
    // we make sure the VRF values if given are between a min and a max.
    int vrf = static_cast<int>(common_field.value().u32());
    CHECK_RETURN_IF_FALSE(vrf >= kVrfMin && vrf <= kVrfMax)
        << "VRF (" << vrf << ") is out of range [" << kVrfMin << ", " << kVrfMax
        << "]. Mapped Field is " << common_field.ShortDebugString() << ".";
    CHECK_RETURN_IF_FALSE(bcm_table_type == BcmFlowEntry::BCM_TABLE_ACL ||
                          !common_field.has_mask())
        << "Non-ACL VRF match fields do not accept a mask value. "
        << "The Mapped Field is " << common_field.ShortDebugString() << ".";
  }

  BcmField::Type bcm_type = GetBcmFieldType(common_field.type());
  if (bcm_type == BcmField::UNKNOWN) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid or unsupported P4 field type to match: "
           << P4FieldType_Name(common_field.type()) << ". Mapped Field is "
           << common_field.ShortDebugString() << ".";
  }
  // We need to convert the port IDs to BCM-specific logical ports for match
  // fields that use ports.
  if (bcm_type == BcmField::IN_PORT || bcm_type == BcmField::CLONE_PORT ||
      bcm_type == BcmField::OUT_PORT) {
    uint64 port_id = common_field.value().u32() + common_field.value().u64();
    const int* bcm_port = gtl::FindOrNull(port_id_to_logical_port_, port_id);
    // Egress ports may refer to a trunk instead. Currently, we do not support
    // ingress trunk matching.
    if (bcm_port == nullptr && bcm_type == BcmField::OUT_PORT) {
      bcm_port = gtl::FindOrNull(trunk_id_to_trunk_port_, port_id);
    }
    if (bcm_port == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Unknown port id: " << port_id
                                           << " on node " << node_id_ << ".";
    }
    MappedField port_id_common_field;
    port_id_common_field.mutable_value()->set_u32(*bcm_port);
    port_id_common_field.mutable_mask()->set_u32(~0);
    FillBcmField(bcm_type, port_id_common_field, bcm_field);
    return ::util::OkStatus();
  }
  // Base scenario is to directly transfer the fields.
  FillBcmField(bcm_type, common_field, bcm_field);
  return ::util::OkStatus();
}

// ConvertSendOrCopyToCpuAction focuses on four action types:
//   CPU_QUEUE_ID, EGRESS_PORT, DROP, CLONE
// Below are exact sets used to determine Copy/Send-to-CPU actions.
//   Skip
//     No CPU_QUEUE_ID + No CPU EGRESS_PORT + Any DROP + No CLONE
//   Copy-to-CPU combinations:
//        CPU_QUEUE_ID + No CPU EGRESS_PORT + Any DROP +    CLONE
//   Send-to-CPU combinations:
//        CPU_QUEUE_ID +    CPU EGRESS_PORT + Any DROP + No CLONE
//   Error
//     All other cases.
// TODO: The clone port (P4_FIELD_TYPE_CLONE_PORT) is a recent
// addition to Hercules P4 programs.  For the current implementation that
// expects all cloning actions to have a CPU target, it provides no additional
// information.  After evolution to PSA, it won't exist - PSA replaces it
// with a clone-session-ID.  However, there may be a short term window where
// P4_FIELD_TYPE_CLONE_PORT needs to be recognized to support new cloning
// and replication features such as "packet chamber" if they are required
// before Hercules supports PSA.
::util::Status BcmTableManager::ConvertSendOrCopyToCpuAction(
    P4ActionFunction* action_function,
    std::vector<BcmAction>* bcm_actions) const {
  // Extract the send/copy-to-cpu related actions.
  P4ActionFunction::P4ActionFields* cpu_queue_action = nullptr;
  P4ActionFunction::P4ActionFields* egress_to_cpu_action = nullptr;
  P4ActionFunction::P4ActionFields* clone_port_action = nullptr;
  for (auto& field : *action_function->mutable_modify_fields()) {
    if (field.type() == P4_FIELD_TYPE_CPU_QUEUE_ID) {
      cpu_queue_action = &field;
    } else if (field.type() == P4_FIELD_TYPE_EGRESS_PORT &&
               (field.u64() == kCpuPortId || field.u32() == kCpuPortId)) {
      egress_to_cpu_action = &field;
    } else if (field.type() == P4_FIELD_TYPE_CLONE_PORT) {
      clone_port_action = &field;
    }
  }
  P4ActionFunction::P4ActionPrimitive* drop_action = nullptr;
  P4ActionFunction::P4ActionPrimitive* clone_action = nullptr;
  for (auto& primitive : *action_function->mutable_primitives()) {
    if (primitive.op_code() == P4_ACTION_OP_DROP) {
      drop_action = &primitive;
    } else if (primitive.op_code() == P4_ACTION_OP_CLONE) {
      clone_action = &primitive;
    }
  }
  if (!cpu_queue_action && !egress_to_cpu_action && !clone_action) {
    return ::util::OkStatus();
  }

  // All Copy/Send to CPU actions require a CPU Queue ID.
  if (cpu_queue_action == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "A P4_FIELD_TYPE_EGRESS_PORT to CPU or a P4_ACTION_OP_CLONE "
              "action was requested but no P4_FIELD_TYPE_CPU_QUEUE_ID action "
              "was provided.";
  }
  if (egress_to_cpu_action && clone_action) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "P4_FIELD_TYPE_EGRESS_PORT to CPU  "
                                            "and P4_ACTION_OP_CLONE cannot "
                                            "both be present as actions.";
  }
  // Grab the set of colors for the copy action.
  gtl::flat_hash_set<P4MeterColor> copy_colors;
  if (clone_action) {
    for (int color : clone_action->meter_colors()) {
      copy_colors.insert(static_cast<P4MeterColor>(color));
    }
  } else {
    for (int color : egress_to_cpu_action->meter_colors()) {
      copy_colors.insert(static_cast<P4MeterColor>(color));
    }
  }
  if (copy_colors.empty()) {
    copy_colors = AllColors();
  }
  // Grab the set of colors for the drop action.
  gtl::flat_hash_set<P4MeterColor> drop_colors;
  if (drop_action) {
    for (int color : drop_action->meter_colors()) {
      drop_colors.insert(static_cast<P4MeterColor>(color));
    }
    if (drop_colors.empty()) {
      drop_colors = AllColors();
    }
  }

  bool send_to_cpu = egress_to_cpu_action;

  // There is a special case for unconditional send-to-cpu actions. If the send
  // action is uncolored, it is deemed to be the inverse of the drop colors.
  if (send_to_cpu && copy_colors.size() == AllColors().size()) {
    if (drop_colors.size() == AllColors().size()) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Cannot process overlapping "
                                              "uncolored drop and "
                                              "send-to-cpu actions.";
    }
    // Remove the drop colors from the copy colors.
    for (P4MeterColor color : drop_colors) {
      copy_colors.erase(color);
    }
  } else if (send_to_cpu) {
    // TODO: This is now a punt table feature in recent Hercules
    // P4 programs.  A cloned and metered copy of the packet goes to the CPU
    // while the original ingress packet gets dropped.  It needs to be supported
    // to comply with the latest P4 specs.
    for (P4MeterColor color : drop_colors) {
      if (copy_colors.count(color)) {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Cannot process overlapping drop and send-to-cpu color "
               << P4MeterColor_Name(static_cast<P4MeterColor>(color)) << " ("
               << color << ") "
               << ".";
      }
    }
  }

  // Merge the copy colors into the drop action when sending to CPU.
  if (send_to_cpu) {
    drop_colors.insert(copy_colors.begin(), copy_colors.end());
  }

  // Set up the COPY_TO_CPU Action.
  BcmAction bcm_copy_action;
  bcm_copy_action.set_type(BcmAction::COPY_TO_CPU);
  // Set up cpu queue for Copy.
  BcmAction::Param* param = bcm_copy_action.add_params();
  param->set_type(BcmAction::Param::QUEUE);
  param->mutable_value()->set_u32(cpu_queue_action->u32());
  // Set up color(s) for Copy. If everything should be copied, don't fill in the
  // color parameters.
  if (copy_colors.size() == AllColors().size()) {
    bcm_actions->emplace_back(std::move(bcm_copy_action));
  } else {
    RETURN_IF_ERROR(
        FillBcmActionColorParams(copy_colors, bcm_copy_action, bcm_actions));
  }

  // We may not need a drop action in a copy-to-cpu with no drop action
  // scenario.
  if (!drop_colors.empty()) {
    // Set up the DROP action.
    BcmAction bcm_drop_action;
    bcm_drop_action.set_type(BcmAction::DROP);
    // Set up color(s) for Drop.
    if (drop_colors.size() == AllColors().size()) {
      bcm_actions->emplace_back(std::move(bcm_drop_action));
    } else {
      RETURN_IF_ERROR(
          FillBcmActionColorParams(drop_colors, bcm_drop_action, bcm_actions));
    }
  }

  // Remove the used actions.
  gtl::flat_hash_set<const ::google::protobuf::Message*> messages = {
      cpu_queue_action, egress_to_cpu_action, clone_action, drop_action,
      clone_port_action};
  messages.erase(nullptr);
  EraseReferencesFromRepeatedField(messages,
                                   action_function->mutable_modify_fields());
  EraseReferencesFromRepeatedField(messages,
                                   action_function->mutable_primitives());

  return ::util::OkStatus();
}

::util::Status BcmTableManager::ConvertNexthopAction(
    P4ActionFunction* action_function,
    std::vector<BcmAction>* bcm_actions) const {
  if (action_function->modify_fields_size() != 3) {
    return ::util::OkStatus();
  }
  P4ActionFunction::P4ActionFields* eth_source_action = nullptr;
  P4ActionFunction::P4ActionFields* eth_dest_action = nullptr;
  P4ActionFunction::P4ActionFields* egress_port_action = nullptr;
  for (auto& field : *action_function->mutable_modify_fields()) {
    switch (field.type()) {
      case P4_FIELD_TYPE_ETH_SRC:
        if (eth_source_action) {
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Cannot process multiple P4_FIELD_TYPE_ETH_SRC actions.";
        }
        eth_source_action = &field;
        break;
      case P4_FIELD_TYPE_ETH_DST:
        if (eth_dest_action) {
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Cannot process multiple P4_FIELD_TYPE_ETH_DST actions.";
        }
        eth_dest_action = &field;
        break;
      case P4_FIELD_TYPE_EGRESS_PORT:
        if (egress_port_action) {
          return MAKE_ERROR(ERR_INVALID_PARAM) << "Cannot process multiple "
                                                  "P4_FIELD_TYPE_EGRESS_PORT "
                                                  "actions.";
        }
        egress_port_action = &field;
        break;
      default:
        break;
    }
  }
  // All actions need to exist for a nexthop.
  if (!eth_source_action || !eth_dest_action || !egress_port_action) {
    return ::util::OkStatus();
  }

  // From now onward, we assume the desired action is a nexthop.
  if (eth_source_action->u64() == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "An ethernet source of 0 cannot be used in a nexthop action.";
  }
  if (eth_dest_action->u64() == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "An ethernet destination of 0 "
                                            "cannot be used in a nexthop "
                                            "action.";
  }
  if (egress_port_action->u32() == kCpuPortId ||
      egress_port_action->u64() == kCpuPortId) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "CPU is not a valid nexthop destination.";
  }

  BcmAction bcm_eth_source_action;
  RETURN_IF_ERROR(
      P4ActionFieldToBcmAction(*eth_source_action, &bcm_eth_source_action));
  BcmAction bcm_eth_dest_action;
  RETURN_IF_ERROR(
      P4ActionFieldToBcmAction(*eth_dest_action, &bcm_eth_dest_action));
  BcmAction bcm_egress_port_action;
  RETURN_IF_ERROR(
      P4ActionFieldToBcmAction(*egress_port_action, &bcm_egress_port_action));

  bcm_actions->push_back(bcm_eth_source_action);
  bcm_actions->push_back(bcm_eth_dest_action);
  bcm_actions->push_back(bcm_egress_port_action);

  // Remove the used actions.
  gtl::flat_hash_set<const ::google::protobuf::Message*> messages = {
      eth_source_action, eth_dest_action, egress_port_action};
  EraseReferencesFromRepeatedField(messages,
                                   action_function->mutable_modify_fields());

  return ::util::OkStatus();
}

::util::Status BcmTableManager::ConvertComplexP4Actions(
    P4ActionFunction* action_function,
    std::vector<BcmAction>* bcm_actions) const {
  RETURN_IF_ERROR(ConvertSendOrCopyToCpuAction(action_function, bcm_actions));
  RETURN_IF_ERROR(ConvertNexthopAction(action_function, bcm_actions));
  return ::util::OkStatus();
}

::util::Status BcmTableManager::P4ActionFieldToBcmAction(
    const P4ActionFunction::P4ActionFields& common_action,
    BcmAction* bcm_action) const {
  switch (common_action.type()) {
    case P4_FIELD_TYPE_ETH_SRC:
      return FillSimpleBcmAction(common_action, BcmAction::SET_ETH_SRC,
                                 BcmAction::Param::ETH_SRC, bcm_action);
    case P4_FIELD_TYPE_ETH_DST:
      return FillSimpleBcmAction(common_action, BcmAction::SET_ETH_DST,
                                 BcmAction::Param::ETH_DST, bcm_action);
    case P4_FIELD_TYPE_VLAN_VID:
      return FillSimpleBcmAction(common_action, BcmAction::SET_VLAN_VID,
                                 BcmAction::Param::VLAN_VID, bcm_action);
    case P4_FIELD_TYPE_VLAN_PCP:
      return FillSimpleBcmAction(common_action, BcmAction::SET_VLAN_PCP,
                                 BcmAction::Param::VLAN_PCP, bcm_action);
    case P4_FIELD_TYPE_IPV4_SRC:
      return FillSimpleBcmAction(common_action, BcmAction::SET_IPV4_SRC,
                                 BcmAction::Param::IPV4_SRC, bcm_action);
    case P4_FIELD_TYPE_IPV4_DST:
      return FillSimpleBcmAction(common_action, BcmAction::SET_IPV4_DST,
                                 BcmAction::Param::IPV4_DST, bcm_action);
    case P4_FIELD_TYPE_IPV6_SRC:
      return FillSimpleBcmAction(common_action, BcmAction::SET_IPV6_SRC,
                                 BcmAction::Param::IPV6_SRC, bcm_action);
    case P4_FIELD_TYPE_IPV6_DST:
      return FillSimpleBcmAction(common_action, BcmAction::SET_IPV6_DST,
                                 BcmAction::Param::IPV6_DST, bcm_action);
    case P4_FIELD_TYPE_VRF:
      return FillSimpleBcmAction(common_action, BcmAction::SET_VRF,
                                 BcmAction::Param::VRF, bcm_action);
    case P4_FIELD_TYPE_CLASS_ID:
      return FillSimpleBcmAction(common_action, BcmAction::SET_VFP_DST_CLASS_ID,
                                 BcmAction::Param::VFP_DST_CLASS_ID,
                                 bcm_action);
    case P4_FIELD_TYPE_COLOR:
      return FillSimpleBcmAction(common_action, BcmAction::SET_COLOR,
                                 BcmAction::Param::COLOR, bcm_action);
    case P4_FIELD_TYPE_MCAST_GROUP_ID:
      return FillSimpleBcmAction(common_action, BcmAction::SET_L2_MCAST_GROUP,
                                 BcmAction::Param::L2_MCAST_GROUP_ID,
                                 bcm_action);
    case P4_FIELD_TYPE_COS:
      return FillSimpleBcmAction(common_action, BcmAction::SET_COS,
                                 BcmAction::Param::COS, bcm_action);
    case P4_FIELD_TYPE_L3_ADMIT:
      // Nothing to do in this case.
      break;
    case P4_FIELD_TYPE_EGRESS_PORT:
    case P4_FIELD_TYPE_EGRESS_TRUNK:
      // Only one of common_action.u64() & common_action.u32() can be non-zero.
      // Constant parameters always show up as u64.
      return CreateEgressPortAction(common_action.u64() + common_action.u32(),
                                    bcm_action);
    case P4_FIELD_TYPE_CPU_QUEUE_ID:
    case P4_FIELD_TYPE_CLONE_PORT:
      // CPU_QUEUE_ID and CLONE_PORT should be dealt with in
      // ConvertSendOrCopyToCpuAction().
      return MAKE_ERROR(ERR_INTERNAL)
             << P4FieldType_Name(common_action.type())
             << " should have been handled as a "
             << "complex action but made it to the individual actions handler.";
    case P4_FIELD_TYPE_ANNOTATED:
    case P4_FIELD_TYPE_NW_TTL:
    case P4_FIELD_TYPE_ETH_TYPE:
    case P4_FIELD_TYPE_IPV4_PROTO:
    case P4_FIELD_TYPE_IPV4_DIFFSERV:
      // This translates to both SET_IP_DSCP & SET_IP_ECN.
    case P4_FIELD_TYPE_IPV6_NEXT_HDR:
    case P4_FIELD_TYPE_IPV6_TRAFFIC_CLASS:
    case P4_FIELD_TYPE_ICMP_TYPE:
    case P4_FIELD_TYPE_ICMP_CODE:
    case P4_FIELD_TYPE_L4_SRC_PORT:
    case P4_FIELD_TYPE_L4_DST_PORT:
    case P4_FIELD_TYPE_ARP_TPA:
    case P4_FIELD_TYPE_INGRESS_PORT:
    case P4_FIELD_TYPE_INGRESS_TRUNK:
    case P4_FIELD_TYPE_IN_METER:
    case P4_FIELD_TYPE_UNKNOWN:
    default:
      return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
             << "P4 Field Type " << P4FieldType_Name(common_action.type())
             << " (" << common_action.type()
             << ") is not supported for actions.";
  }

  return ::util::OkStatus();
}

::util::StatusOr<BcmFlowEntry::BcmTableType> BcmTableManager::GetBcmTableType(
    const CommonFlowEntry& common_flow_entry) const {
  uint32 table_id = common_flow_entry.table_info().id();
  P4TableType table_type = common_flow_entry.table_info().type();
  P4Annotation::PipelineStage pipeline_stage =
      common_flow_entry.table_info().pipeline_stage();

  // We always expect the stage to be available for any table entry.
  CHECK_RETURN_IF_FALSE(pipeline_stage != P4Annotation::DEFAULT_STAGE)
      << "Invalid stage for the table entry: "
      << common_flow_entry.ShortDebugString();

  // Handle ACL tables.
  if (acl_tables_.count(table_id)) {
    return BcmFlowEntry::BCM_TABLE_ACL;
  }

  BcmFlowEntry::BcmTableType bcm_table_type = BcmFlowEntry::BCM_TABLE_UNKNOWN;
  switch (table_type) {
    case P4_TABLE_L3_IP: {
      // In this case, we expect either P4_FIELD_TYPE_IPV4_DST or
      // P4_FIELD_TYPE_IPV6_DST in the set of field. If not, something might be
      // wrong.
      bool ipv4 = false, ipv6 = false;
      for (const auto& field : common_flow_entry.fields()) {
        switch (field.type()) {
          case P4_FIELD_TYPE_IPV4_DST:
            ipv4 = true;
            break;
          case P4_FIELD_TYPE_IPV6_DST:
            ipv6 = true;
            break;
          default:
            break;
        }
      }
      CHECK_RETURN_IF_FALSE((ipv4 && !ipv6) || (!ipv4 && ipv6))
          << "The L3 LPM flow is neither IPv4 nor IPv6. CommonFlowEntry is "
          << common_flow_entry.ShortDebugString() << ".";
      if (ipv4) {
        bcm_table_type = BcmFlowEntry::BCM_TABLE_IPV4_LPM;
      } else if (ipv6) {
        bcm_table_type = BcmFlowEntry::BCM_TABLE_IPV6_LPM;
      }
      break;
    }
    case P4_TABLE_L3_CLASSIFIER:
      // TODO: Seems like this is not used any more in the new
      // P4 pipeline configs. Remove if not needed.
      bcm_table_type = BcmFlowEntry::BCM_TABLE_MY_STATION;
      break;
    default:
      break;
  }

  // If table_type is not assigned in a common_flow_entry, we fallback to
  // pipeline stage and based on that possibly the field/action types try to
  // infer the type of the table.
  if (bcm_table_type == BcmFlowEntry::BCM_TABLE_UNKNOWN) {
    switch (pipeline_stage) {
      case P4Annotation::L2: {
        // Now we need to rely on the fields and actions to understand the
        // table type. Only P4_ACTION_TYPE_FUNCTION action type make sense in
        // this case.
        if (common_flow_entry.action().type() == P4_ACTION_TYPE_FUNCTION) {
          P4ActionFunction function = common_flow_entry.action().function();
          for (const auto& field : function.modify_fields()) {
            if (field.type() == P4_FIELD_TYPE_MCAST_GROUP_ID) {
              bcm_table_type = BcmFlowEntry::BCM_TABLE_L2_MULTICAST;
              break;
            }
            if (field.type() == P4_FIELD_TYPE_L3_ADMIT) {
              bcm_table_type = BcmFlowEntry::BCM_TABLE_MY_STATION;
              break;
            }
          }
        }
        break;
      }
      default:
        break;
    }
  }

  CHECK_RETURN_IF_FALSE(bcm_table_type != BcmFlowEntry::BCM_TABLE_UNKNOWN)
      << "Could not find BCM table id from "
      << common_flow_entry.ShortDebugString();

  return bcm_table_type;
}

util::StatusOr<BcmFlowTable*> BcmTableManager::GetMutableFlowTable(
    uint32 table_id) {
  auto generic_lookup = generic_flow_tables_.find(table_id);
  if (generic_lookup != generic_flow_tables_.end()) {
    return &(generic_lookup->second);
  }
  auto acl_lookup = acl_tables_.find(table_id);
  if (acl_lookup != acl_tables_.end()) {
    return &(acl_lookup->second);
  }
  return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
         << "Table " << table_id << " not present.";
}

util::StatusOr<const BcmFlowTable*> BcmTableManager::GetConstantFlowTable(
    uint32 table_id) const {
  const auto generic_lookup = generic_flow_tables_.find(table_id);
  if (generic_lookup != generic_flow_tables_.end()) {
    return &(generic_lookup->second);
  }
  const auto acl_lookup = acl_tables_.find(table_id);
  if (acl_lookup != acl_tables_.end()) {
    return &(acl_lookup->second);
  }

  return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
         << "Table " << table_id << " not present.";
}

bool BcmTableManager::HasTable(uint32 table_id) const {
  if (generic_flow_tables_.count(table_id)) return true;
  if (acl_tables_.count(table_id)) return true;
  return false;
}

bool BcmTableManager::IsAclTable(uint32 table_id) const {
  return acl_tables_.count(table_id) > 0;
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
