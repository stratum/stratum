// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file contains the TableMapGenerator implementation.

#include "stratum/p4c_backends/fpm/table_map_generator.h"

#include "stratum/glue/logging.h"
#include "google/protobuf/util/message_differencer.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/p4_model_names.pb.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "p4/config/v1/p4info.pb.h"

namespace stratum {
namespace p4c_backends {

TableMapGenerator::TableMapGenerator()
    : generated_map_(new hal::P4PipelineConfig) {
}

TableMapGenerator::~TableMapGenerator() {
}

// AddField allows the same field to be added repeatedly.  This behavior
// supports simpler backend behavior in cases where processing one type of
// IR object is not aware that another IR object has already detected the field.
// For example, match key processing detects a field without knowing whether
// parser state processing found the field earlier.
void TableMapGenerator::AddField(const std::string& field_name) {
  hal::P4FieldDescriptor* field_descriptor =
      FindMutableFieldDescriptorOrNull(field_name, generated_map_.get());
  if (field_descriptor == nullptr) {
    hal::P4TableMapValue new_field;
    new_field.mutable_field_descriptor()->set_type(P4_FIELD_TYPE_ANNOTATED);
    (*generated_map_->mutable_table_map())[field_name] = new_field;
  } else {
    VLOG(1) << "Reusing table map entry for field " << field_name;
  }
}

void TableMapGenerator::SetFieldType(const std::string& field_name,
                                     P4FieldType type) {
  SetFieldAttributes(field_name, type, P4_HEADER_UNKNOWN, 0, 0);
}

void TableMapGenerator::SetFieldAttributes(
    const std::string& field_name, P4FieldType field_type,
    P4HeaderType header_type, uint32_t bit_offset, uint32_t bit_width) {
  hal::P4FieldDescriptor* field_descriptor =
      FindMutableFieldDescriptorOrNull(field_name, generated_map_.get());
  if (field_descriptor == nullptr) {
    // TODO(unknown): Treat as internal compiler BUG exception?
    LOG(ERROR) << "Unable to find field " << field_name << " to set attributes";
    return;
  }

  if (field_type != P4_FIELD_TYPE_UNKNOWN) {
    field_descriptor->set_type(field_type);
  }
  if (header_type != P4_HEADER_UNKNOWN) {
    field_descriptor->set_header_type(header_type);
  }
  uint32_t current_offset = field_descriptor->bit_offset();
  if (bit_offset != current_offset) {
    if (current_offset != 0) {
      LOG(ERROR) << "Unexpected bit offset change from " << current_offset
                 << " to " << bit_offset << " for field " << field_name;
    }
    field_descriptor->set_bit_offset(bit_offset);
  }
  uint32_t current_width = field_descriptor->bit_width();
  if (bit_width != current_width) {
    if (current_width != 0) {
      LOG(ERROR) << "Unexpected bit width change from " << current_width
                 << " to " << bit_width << " for field " << field_name;
    }
    field_descriptor->set_bit_width(bit_width);
  }
}

void TableMapGenerator::SetFieldLocalMetadataFlag(
    const std::string& field_name) {
  hal::P4FieldDescriptor* field_descriptor =
      FindMutableFieldDescriptorOrNull(field_name, generated_map_.get());
  if (field_descriptor == nullptr) {
    // TODO(unknown): Treat as internal compiler BUG exception?
    LOG(ERROR) << "Unable to find field " << field_name
               << " to set local metadata flag";
    return;
  }

  field_descriptor->set_is_local_metadata(true);
}

void TableMapGenerator::SetFieldValueSet(
    const std::string& field_name, const std::string& value_set_name,
    P4HeaderType header_type) {
  hal::P4FieldDescriptor* field_descriptor =
      FindMutableFieldDescriptorOrNull(field_name, generated_map_.get());
  if (field_descriptor == nullptr) {
    // TODO(unknown): Treat as internal compiler BUG exception?
    LOG(ERROR) << "Unable to find field " << field_name
               << " to set value set name";
    return;
  }

  field_descriptor->set_value_set(value_set_name);
  field_descriptor->set_type(P4_FIELD_TYPE_UDF_VALUE_SET);
  field_descriptor->set_header_type(header_type);
}

void TableMapGenerator::AddFieldMatch(
    const std::string& field_name,
    const std::string& match_type, int bit_width) {
  hal::P4FieldDescriptor* field_descriptor =
      FindMutableFieldDescriptorOrNull(field_name, generated_map_.get());
  if (field_descriptor == nullptr) {
    // TODO(unknown): Treat as internal compiler BUG exception?
    LOG(ERROR) << "Unable to find field " << field_name << " to add match data";
    return;
  }

  hal::P4FieldDescriptor::P4FieldValueConversion value_conversion =
      hal::P4FieldDescriptor::P4_CONVERT_UNKNOWN;
  ::p4::config::v1::MatchField::MatchType p4_match_type =
      ::p4::config::v1::MatchField::UNSPECIFIED;

  const P4ModelNames& p4_model_names = GetP4ModelNames();
  if (match_type == p4_model_names.exact_match()) {
    p4_match_type = ::p4::config::v1::MatchField::EXACT;
  } else if (match_type == p4_model_names.lpm_match()) {
    p4_match_type = ::p4::config::v1::MatchField::LPM;
  } else if (match_type == p4_model_names.ternary_match()) {
    p4_match_type = ::p4::config::v1::MatchField::TERNARY;
  } else if (match_type == p4_model_names.range_match()) {
    ::error(
        "Backend: Stratum FPM does not support P4 range matches. "
        "Field name: %s",
        field_name);
  } else if (match_type == p4_model_names.selector_match()) {
    // TODO(unknown): Needs more implementation.
  } else {
    // TODO(unknown): Needs more implementation.
  }

  if (bit_width <= 32) {
    if (p4_match_type == ::p4::config::v1::MatchField::EXACT)
      value_conversion = hal::P4FieldDescriptor::P4_CONVERT_TO_U32;
    else
      value_conversion = hal::P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK;
  } else if (bit_width <= 64) {
    if (p4_match_type == ::p4::config::v1::MatchField::EXACT)
      value_conversion = hal::P4FieldDescriptor::P4_CONVERT_TO_U64;
    else
      value_conversion = hal::P4FieldDescriptor::P4_CONVERT_TO_U64_AND_MASK;
  } else {
    if (p4_match_type == ::p4::config::v1::MatchField::EXACT)
      value_conversion = hal::P4FieldDescriptor::P4_CONVERT_TO_BYTES;
    else
      value_conversion = hal::P4FieldDescriptor::P4_CONVERT_TO_BYTES_AND_MASK;
  }

  // It is OK if some other match has already defined this match conversion.
  // The bit_width for the conversion should be the same as the overall
  // field width in the descriptor, if known.
  if (bit_width != field_descriptor->bit_width()) {
    if (field_descriptor->bit_width() == 0) {
      field_descriptor->set_bit_width(bit_width);
    } else {
      LOG(ERROR) << "Unexpected use of field " << field_name << " with width "
                 << field_descriptor->bit_width() << " as match key with width "
                 << bit_width;
      return;
    }
  }

  bool found = false;
  for (auto& conversion : field_descriptor->valid_conversions()) {
    if (conversion.match_type() == p4_match_type) {
      found = true;
      break;
    }
  }

  if (!found) {
    auto new_conversion = field_descriptor->add_valid_conversions();
    new_conversion->set_match_type(p4_match_type);
    new_conversion->set_conversion(value_conversion);
  }
}

void TableMapGenerator::ReplaceFieldDescriptor(
    const std::string& field_name,
    const hal::P4FieldDescriptor& new_descriptor) {
  hal::P4FieldDescriptor* field_descriptor =
      FindMutableFieldDescriptorOrNull(field_name, generated_map_.get());
  if (field_descriptor == nullptr) return;
  *field_descriptor = new_descriptor;
}

// Some actions may be added twice.  This occurs normally when the caller
// recursively processes action statements.  This code ignores repeat
// appearances of action_name.
void TableMapGenerator::AddAction(const std::string& action_name) {
  auto iter = generated_map_->mutable_table_map()->find(action_name);
  if (iter == generated_map_->mutable_table_map()->end()) {
    hal::P4TableMapValue new_action;
    new_action.mutable_action_descriptor()->set_type(P4_ACTION_TYPE_FUNCTION);
    (*generated_map_->mutable_table_map())[action_name] = new_action;
  } else {
    VLOG(1) << "Reusing table map entry for action " << action_name;
  }
}

void TableMapGenerator::AssignActionSourceValueToField(
    const std::string& action_name,
    const P4AssignSourceValue& source_value,
    const std::string& field_name) {
  if (source_value.source_value_case() ==
      P4AssignSourceValue::SOURCE_VALUE_NOT_SET) {
    LOG(ERROR) << "Input source_value is not set "
               << source_value.ShortDebugString();
    return;
  }

  auto action_descriptor = FindActionDescriptor(action_name);
  if (action_descriptor == nullptr)
    return;
  hal::P4ActionDescriptor::P4ActionInstructions* assign_param =
      action_descriptor->add_assignments();
  *assign_param->mutable_assigned_value() = source_value;
  assign_param->set_destination_field_name(field_name);
}

void TableMapGenerator::AssignActionParameterToField(
    const std::string& action_name, const std::string& param_name,
    const std::string& field_name) {
  P4AssignSourceValue source_value;
  source_value.set_parameter_name(param_name);
  AssignActionSourceValueToField(action_name, source_value, field_name);
}

void TableMapGenerator::AssignHeaderToHeader(
      const std::string& action_name, const P4AssignSourceValue& source_header,
      const std::string& destination_header) {
  AssignActionSourceValueToField(
      action_name, source_header, destination_header);
}

void TableMapGenerator::AddDropPrimitive(const std::string& action_name) {
  auto action_descriptor = FindActionDescriptor(action_name);
  if (action_descriptor == nullptr)
    return;
  action_descriptor->add_primitive_ops(P4_ACTION_OP_DROP);
}

void TableMapGenerator::AddNopPrimitive(const std::string& action_name) {
  auto action_descriptor = FindActionDescriptor(action_name);
  if (action_descriptor == nullptr)
    return;
  action_descriptor->add_primitive_ops(P4_ACTION_OP_NOP);
}

void TableMapGenerator::AddMeterColorAction(
    const std::string& action_name,
    const hal::P4ActionDescriptor::P4MeterColorAction& color_action) {
  auto action_descriptor = FindActionDescriptor(action_name);
  if (action_descriptor == nullptr)
    return;
  int color_action_index = FindColorAction(*action_descriptor, color_action);

  if (color_action_index >= 0) {
    hal::P4ActionDescriptor::P4MeterColorAction* append_color_action =
        action_descriptor->mutable_color_actions(color_action_index);
    for (const auto& meter_op : color_action.ops()) {
      // TODO(unknown): Should this check for duplication of a meter_op
      // that's already present in the action descriptor?
      *(append_color_action->add_ops()) = meter_op;
    }
  } else {
    *(action_descriptor->add_color_actions()) = color_action;
  }
}

void TableMapGenerator::AddMeterColorActionsFromString(
    const std::string& action_name, const std::string& color_actions) {
  hal::P4ActionDescriptor parsed_actions;
  if (!ParseProtoFromString(color_actions, &parsed_actions).ok()) {
    LOG(ERROR) << "Unable to parse color_actions string: " << color_actions;
    return;
  }
  for (const auto& color_action : parsed_actions.color_actions()) {
    AddMeterColorAction(action_name, color_action);
  }
}

// TableMapGenerator assumes that the caller adds encap/decap operations in
// the proper sequence, and it is not necessary to filter duplicates.
void TableMapGenerator::AddTunnelAction(
    const std::string& action_name,
    const hal::P4ActionDescriptor::P4TunnelAction& tunnel_action) {
  auto action_descriptor = FindActionDescriptor(action_name);
  if (action_descriptor == nullptr)
    return;
  *(action_descriptor->add_tunnel_actions()) = tunnel_action;
}

void TableMapGenerator::ReplaceActionDescriptor(
    const std::string& action_name,
    const hal::P4ActionDescriptor& new_descriptor) {
  auto action_descriptor = FindActionDescriptor(action_name);
  if (action_descriptor == nullptr)
    return;
  *action_descriptor = new_descriptor;
}

// AddTable allows the same table to be added repeatedly.  This behavior
// supports simpler backend behavior in cases where processing one type of
// IR object is not aware that another IR object has already detected the table.
void TableMapGenerator::AddTable(const std::string& table_name) {
  auto iter = generated_map_->mutable_table_map()->find(table_name);
  if (iter == generated_map_->mutable_table_map()->end()) {
    hal::P4TableMapValue new_table;
    new_table.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
    (*generated_map_->mutable_table_map())[table_name] = new_table;
  } else {
    // TODO(unknown): When this occurs, the entry should be checked to make
    // sure it has a table_descriptor, so it's not a conflict between an action
    // or field name and the table name.  Field and Action adds should do the
    // same thing relative to their descriptor types.
    VLOG(1) << "Reusing table map entry for table " << table_name;
  }
}

void TableMapGenerator::SetTableType(const std::string& table_name,
                                     P4TableType type) {
  auto iter = generated_map_->mutable_table_map()->find(table_name);
  if (iter == generated_map_->mutable_table_map()->end()) {
    // TODO(unknown): Treat as internal compiler BUG exception?
    LOG(ERROR) << "Unable to find table " << table_name << " to set type";
    return;
  }
  iter->second.mutable_table_descriptor()->set_type(type);
}

void TableMapGenerator::SetTableStaticEntriesFlag(
    const std::string& table_name) {
  auto iter = generated_map_->mutable_table_map()->find(table_name);
  if (iter == generated_map_->mutable_table_map()->end()) {
    // TODO(unknown): Treat as internal compiler BUG exception?
    LOG(ERROR) << "Unable to find table " << table_name
               << " to set static entry flag";
    return;
  }
  iter->second.mutable_table_descriptor()->set_has_static_entries(true);
}

void TableMapGenerator::SetTableValidHeaders(
    const std::string& table_name, const std::set<std::string>& header_names) {
  auto iter = generated_map_->mutable_table_map()->find(table_name);
  if (iter == generated_map_->mutable_table_map()->end()) {
    // TODO(unknown): Treat as internal compiler BUG exception?
    LOG(ERROR) << "Unable to find table " << table_name
               << " to set valid headers";
    return;
  }

  auto table_descriptor = iter->second.mutable_table_descriptor();
  table_descriptor->clear_valid_headers();
  for (const auto& header_name : header_names) {
    auto hdr_iter = generated_map_->mutable_table_map()->find(header_name);
    if (hdr_iter == generated_map_->mutable_table_map()->end()) {
      LOG(ERROR) << "Unable to find header " << header_name
                 << " to set valid header type for table " << table_name;
      continue;
    }
    table_descriptor->add_valid_headers(
        hdr_iter->second.header_descriptor().type());
  }
}

void TableMapGenerator::AddHeader(const std::string& header_name) {
  auto iter = generated_map_->mutable_table_map()->find(header_name);
  if (iter == generated_map_->mutable_table_map()->end()) {
    hal::P4TableMapValue new_header;
    new_header.mutable_header_descriptor()->set_type(P4_HEADER_UNKNOWN);
    (*generated_map_->mutable_table_map())[header_name] = new_header;
  } else {
    VLOG(1) << "Reusing table map entry for header " << header_name;
  }
}

void TableMapGenerator::SetHeaderAttributes(
    const std::string& header_name, P4HeaderType type, int32 depth) {
  auto iter = generated_map_->mutable_table_map()->find(header_name);
  if (iter == generated_map_->mutable_table_map()->end()) {
    // TODO(unknown): Treat as internal compiler BUG exception?
    LOG(ERROR) << "Unable to find header " << header_name
               << " to set attributes";
    return;
  }

  if (type != P4_HEADER_UNKNOWN) {
    iter->second.mutable_header_descriptor()->set_type(type);
  }
  if (depth != 0) {
    iter->second.mutable_header_descriptor()->set_depth(depth);
  }
}

void TableMapGenerator::AddInternalAction(
    const std::string& action_name,
    const hal::P4ActionDescriptor& internal_descriptor) {
  auto iter = generated_map_->mutable_table_map()->find(action_name);
  if (iter != generated_map_->mutable_table_map()->end()) {
    LOG(ERROR) << "Unexpected reuse of table map entry for internal action "
               << action_name;
  }

  hal::P4TableMapValue new_internal_action;
  *new_internal_action.mutable_internal_action() = internal_descriptor;
  (*generated_map_->mutable_table_map())[action_name] = new_internal_action;
}

hal::P4ActionDescriptor* TableMapGenerator::FindActionDescriptor(
      const std::string& action_name) {
  auto iter = generated_map_->mutable_table_map()->find(action_name);
  if (iter == generated_map_->mutable_table_map()->end()) {
    LOG(ERROR) << "Unable to find action " << action_name
               << " in generated map data";
    return nullptr;
  }

  hal::P4TableMapValue& action_entry = iter->second;
  if (!action_entry.has_action_descriptor()) {
    // TODO(unknown): Treat as internal compiler BUG exception?
    LOG(ERROR) << "Missing action descriptor for " << action_name;
    return nullptr;
  }

  return action_entry.mutable_action_descriptor();
}

int TableMapGenerator::FindColorAction(
    const hal::P4ActionDescriptor& action_descriptor,
    const hal::P4ActionDescriptor::P4MeterColorAction& color_action) const {
  ::google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      ::google::protobuf::util::MessageDifferencer::AS_SET);
  const ::google::protobuf::Descriptor* proto_descriptor =
        hal::P4ActionDescriptor::
                      P4MeterColorAction::default_instance().GetDescriptor();
  msg_differencer.IgnoreField(proto_descriptor->FindFieldByName("ops"));

  for (int i = 0; i < action_descriptor.color_actions_size(); ++i) {
    if (msg_differencer.Compare(action_descriptor.color_actions(i),
        color_action))
      return i;
  }
  return -1;
}

}  // namespace p4c_backends
}  // namespace stratum
