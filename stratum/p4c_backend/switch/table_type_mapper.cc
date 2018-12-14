// This file implements the TableTypeMapper.

#include "platforms/networking/hercules/p4c_backend/switch/table_type_mapper.h"

#include "base/logging.h"
#include "platforms/networking/hercules/p4c_backend/switch/utils.h"
#include "platforms/networking/hercules/public/proto/p4_annotation.host.pb.h"

namespace google {
namespace hercules {
namespace p4c_backend {

TableTypeMapper::TableTypeMapper()
    : new_table_type_(P4_TABLE_UNKNOWN),
      found_table_type_(P4_TABLE_UNKNOWN) {
}

// TODO(teverman): ProcessTables is currently hard-coded to decide table type
// based on certain table attributes.  A potentially more general and longer
// term solution could match the P4Info and pipeline config table data to a
// target-specific text file description of fixed pipeline table attributes.
void TableTypeMapper::ProcessTables(const hal::P4InfoManager& p4_info_manager,
                                    hal::P4PipelineConfig* p4_pipeline_config) {
  for (const auto& p4_info_table : p4_info_manager.p4_info().tables()) {
    current_table_name_ = p4_info_table.preamble().name();
    auto table_descriptor = FindMutableTableDescriptorOrDie(
        current_table_name_, p4_pipeline_config);

    if (table_descriptor->pipeline_stage() != P4Annotation::L2) continue;
    if (table_descriptor->type() != P4_TABLE_UNKNOWN) continue;
    new_table_type_ = P4_TABLE_UNKNOWN;
    found_table_type_ = P4_TABLE_UNKNOWN;

    for (const auto& action_ref : p4_info_table.action_refs()) {
      auto action_status = p4_info_manager.FindActionByID(action_ref.id());
      CHECK(action_status.ok())
          << "Unexpected failure to find P4Info for action ID "
          << action_ref.id();
      const ::p4::config::v1::Action& p4_info_action =
          action_status.ValueOrDie();
      const auto& action_descriptor = FindActionDescriptorOrDie(
          p4_info_action.preamble().name(), *p4_pipeline_config);
      GetL2TableTypeFromAction(action_descriptor, *p4_pipeline_config);
    }

    table_descriptor->set_type(new_table_type_);
  }
}

void TableTypeMapper::GetL2TableTypeFromAction(
    const hal::P4ActionDescriptor& action_descriptor,
    const hal::P4PipelineConfig& p4_pipeline_config) {
  for (const auto& assignment : action_descriptor.assignments()) {
    if (!assignment.destination_field_name().empty()) {
      const auto& dest_field = assignment.destination_field_name();
      const hal::P4FieldDescriptor* field_descriptor =
          FindFieldDescriptorOrNull(dest_field, p4_pipeline_config);

      // Failure to find a field descriptor is possible when the assignment
      // is a header-to-header copy and the destination field has a header
      // descriptor instead.
      if (field_descriptor == nullptr) continue;
      switch (field_descriptor->type()) {
        case P4_FIELD_TYPE_MCAST_GROUP_ID:
          ProposeNewTableType(P4_TABLE_L2_MULTICAST);
          break;
        case P4_FIELD_TYPE_L3_ADMIT:
          ProposeNewTableType(P4_TABLE_L2_MY_STATION);
          break;
        default:
          break;
      }
    }
  }
}

void TableTypeMapper::ProposeNewTableType(P4TableType proposed_table_type) {
  if (proposed_table_type != new_table_type_) {
    if (found_table_type_ == P4_TABLE_UNKNOWN) {
      new_table_type_ = proposed_table_type;
      found_table_type_ = new_table_type_;
    } else {
      LOG(WARNING) << "Table " << current_table_name_
                   << " has a table type conflict between "
                   << P4TableType_Name(proposed_table_type) << " and "
                   << P4TableType_Name(found_table_type_);
      new_table_type_ = P4_TABLE_UNKNOWN;
    }
  }
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
