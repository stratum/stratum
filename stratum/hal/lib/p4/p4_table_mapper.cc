// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/p4/p4_table_mapper.h"

#include <set>

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/net_util/ipaddress.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/p4/p4_config_verifier.h"
#include "stratum/hal/lib/p4/p4_match_key.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"
#include "stratum/glue/gtl/map_util.h"

// This is the bit width of an assigned constant for any case where the
// compiler does not report a bit width in the action descriptor.
DEFINE_int32(p4c_constant_bitwidth, 64,
             "Bitwidth assigned to p4c constant expression output");

namespace stratum {
namespace hal {

P4TableMapper::P4TableMapper()
    : static_entry_mapper_(absl::make_unique<P4StaticEntryMapper>(this)),
      static_table_updates_enabled_(false),
      node_id_(0) {}

P4TableMapper::~P4TableMapper() { Shutdown().IgnoreError(); }

::util::Status P4TableMapper::PushChassisConfig(const ChassisConfig& config,
                                                uint64 node_id) {
  // TODO(unknown): Use the given ChassisConfig proto if needed.
  node_id_ = node_id;
  return ::util::OkStatus();
}

::util::Status P4TableMapper::VerifyChassisConfig(const ChassisConfig& config,
                                                  uint64 node_id) {
  // TODO(unknown): Implement if needed.
  return ::util::OkStatus();
}

::util::Status P4TableMapper::PushForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  const ::p4::config::v1::P4Info& p4_info = config.p4info();
  const std::string& p4_device_config = config.p4_device_config();

  // PushForwardingPipelineConfig uses the input P4Info and the target-specific
  // spec from the config to do map setup.
  std::unique_ptr<P4InfoManager> p4_info_manager =
      absl::make_unique<P4InfoManager>(p4_info);
  RETURN_IF_ERROR(p4_info_manager->InitializeAndVerify());

  // The p4_device_config byte stream in this case is nothing but the
  // serialized version of P4PipelineConfig.
  P4PipelineConfig p4_pipeline_config;
  CHECK_RETURN_IF_FALSE(p4_pipeline_config.ParseFromString(p4_device_config))
      << "Failed to parse p4_device_config byte stream to P4PipelineConfig.";

  // If there is no change in the forwarding pipeline config pushed to the node,
  // dont do anything.
  if (p4_info_manager_ != nullptr &&
      ProtoEqual(p4_info_manager->p4_info(), p4_info_manager_->p4_info()) &&
      ProtoEqual(p4_pipeline_config, p4_pipeline_config_)) {
    LOG(INFO) << "Forwarding pipeline config is unchanged. Skipped!";
    return ::util::OkStatus();
  }

  // TODO(unknown): If the old pushed forwarding pipeline config needs to be
  // examined to handle the diff, do this here. At the moment, there is no
  // need to do this though. We recreate the state from scratch as part of any
  // new config push.

  // Cleanup the internal maps.
  ClearMaps();

  // Update p4_pipeline_config_ & p4_info_manager_ based on the newly pushed
  // forwarding pipeline config.
  p4_pipeline_config_ = p4_pipeline_config;
  p4_info_manager_ = std::move(p4_info_manager);

  // Each P4 object in the P4Info should have mapping data. A link between
  // the mapping data and the P4 object ID gets created here. This function
  // assumes that P4InfoManager has already verified the validity of name and
  // ID fields in each object's preamble.
  // TODO(unknown): Only match fields and actions are mapped now; add others.
  for (const auto& action : p4_info.actions()) {
    ::util::Status status = AddMapEntryFromPreamble(action.preamble());
    if (!status.ok()) {
      ClearMaps();
      return status;
    }
  }

  // Three things need to be done for each table in P4Info:
  //  1) Set up the P4 table ID to physical table mapping.
  //  2) Determine the table-specific header field conversion that applies
  //     to each match field.
  //  3) Establish a correspondence between the table and its valid actions.
  param_mapper_ = absl::make_unique<P4ActionParamMapper>(
      *p4_info_manager_, global_id_table_map_, p4_pipeline_config_);

  for (const auto& table : p4_info.tables()) {
    ::util::Status table_status = AddMapEntryFromPreamble(table.preamble());
    if (!table_status.ok()) {
      // Since there are discrepancies caused by hidden p4c internal objects
      // that sometimes appear in the output, this error just causes a warning.
      LOG(WARNING) << "Skipping table " << table.preamble().name() << " with no"
                   << " table descriptor in the forwarding pipeline spec";
      continue;
    }

    for (const auto& match_field : table.match_fields()) {
      if (match_field.name().empty()) {
        LOG(WARNING) << "Match field " << match_field.ShortDebugString()
                     << " in table " << table.preamble().name()
                     << " has no name - P4Info may be obsolete";
        continue;
      }
      auto field_desc_iter =
          p4_pipeline_config_.table_map().find(match_field.name());
      if (field_desc_iter != p4_pipeline_config_.table_map().end()) {
        const auto& field_descriptor =
            field_desc_iter->second.field_descriptor();
        auto match_type = match_field.match_type();
        bool conversion_found = false;
        for (const auto& conversion : field_descriptor.valid_conversions()) {
          if (match_type == conversion.match_type() &&
              match_field.bitwidth() == field_descriptor.bit_width()) {
            P4FieldConvertKey key = MakeP4FieldConvertKey(table, match_field);
            P4FieldConvertValue value;
            value.conversion_entry = conversion;
            value.mapped_field.set_type(field_descriptor.type());
            value.mapped_field.set_bit_offset(field_descriptor.bit_offset());
            value.mapped_field.set_bit_width(field_descriptor.bit_width());
            value.mapped_field.set_header_type(field_descriptor.header_type());
            field_convert_by_table_[key] = value;
            conversion_found = true;
            break;
          }
        }
        if (!conversion_found) {
          // TODO(unknown): For now, assume this is due to in-progress
          // table map file development.
          LOG(WARNING) << "Match field " << match_field.ShortDebugString()
                       << " in table " << table.preamble().name()
                       << " has no known mapping conversion";
        }
      } else {
        // TODO(unknown): Not all fields are defined yet, so just warn.
        LOG(WARNING) << "P4TableMapper is ignoring match field "
                     << match_field.ShortDebugString() << " in table "
                     << table.preamble().name();
        continue;
      }
    }

    // For each of the table's action IDs, the param_mapper_ sets up the
    // mappings needed to decode the action's parameters.
    // - Create a map/set indicating all valid combinations.
    for (const auto& action_ref : table.action_refs()) {
      // TODO(unknown): For now, assume any non-OK status is due to
      // in-progress table map file development.
      auto action_status =
          param_mapper_->AddAction(table.preamble().id(), action_ref.id());
      if (!action_status.ok()) {
        LOG(WARNING) << "P4TableMapper has incomplete mapping for "
                     << "action " << PrintP4ObjectID(action_ref.id())
                     << " in table " << table.preamble().name();
      }
    }
  }

  // Parse controller metadata and populate the internal tables. We try our
  // best to parse metadata and skip invalid/unknown data.
  for (const auto& controller_packet_metadata :
       p4_info.controller_packet_metadata()) {
    // Unfortunately other than parsing the names, there is no better way to
    // distinguish packet in vs packet out metadata.
    // TODO(unknown): Find a better way to distinguish packet in vs out.
    const std::string& name = controller_packet_metadata.preamble().name();
    if (name != kIngressMetadataPreambleName &&
        name != kEgressMetadataPreambleName) {
      LOG(WARNING) << "Skipped unknown metadata preamble: " << name << ".";
      continue;
    }
    for (const auto& metadata : controller_packet_metadata.metadata()) {
      // P4Info metadata field names are not fully qualified, so p4c
      // synthesizes unique keys for their table map entries by adding
      // the metadata preamble name as a prefix.
      const std::string metadata_key = name + "." + metadata.name();
      const P4TableMapValue* value =
          gtl::FindOrNull(p4_pipeline_config_.table_map(), metadata_key);
      if (value == nullptr) {
        LOG(WARNING) << "Cannot find the following metadata name as key in "
                     << "p4_pipeline_config_: " << metadata.ShortDebugString()
                     << ". Skipped.";
        continue;
      }
      const auto& type = value->field_descriptor().type();
      if (type == P4_FIELD_TYPE_UNKNOWN) {
        LOG(WARNING) << "Unknown type for the following metadata: "
                     << metadata.ShortDebugString() << ". Skipped.";
        continue;
      }
      uint32 id = metadata.id();
      int bitwidth = metadata.bitwidth();
      if (name == kIngressMetadataPreambleName) {
        if (!gtl::InsertIfNotPresent(
                &packetin_metadata_id_to_type_bitwidth_pair_, id,
                std::make_pair(type, bitwidth))) {
          LOG(WARNING) << "Metadata with ID " << id << " already exists in "
                       << "packetin_metadata_id_to_type_bitwidth_pair_.";
        }
        if (!gtl::InsertIfNotPresent(
                &packetin_metadata_type_to_id_bitwidth_pair_, type,
                std::make_pair(id, bitwidth))) {
          LOG(WARNING) << "Metadata with type " << type << " already exists in "
                       << "packetin_metadata_type_to_id_bitwidth_pair_.";
        }
      } else {
        if (!gtl::InsertIfNotPresent(
                &packetout_metadata_id_to_type_bitwidth_pair_, id,
                std::make_pair(type, bitwidth))) {
          LOG(WARNING) << "Metadata with ID " << id << " already exists in "
                       << "packetout_metadata_id_to_type_bitwidth_pair_.";
        }
        if (!gtl::InsertIfNotPresent(
                &packetout_metadata_type_to_id_bitwidth_pair_, type,
                std::make_pair(id, bitwidth))) {
          LOG(WARNING) << "Metadata with type " << type << " already exists in "
                       << "packetout_metadata_type_to_id_bitwidth_pair_.";
        }
      }
    }
  }

  return ::util::OkStatus();
}

// This function should iterate all objects in P4Info and make sure they have
// table map entries.  At present, it doesn't care about unmapped objects.
// This is a short term development short cut so that only a limited number
// of interesting P4 objects need to be mapped.
// TODO(unknown): Address this longer term as a the switch implementation
// supports a broader set of P4 objects.  This is getting closer to reality.
// The current obstacle is the treatment of internal objects that p4c fails
// to hide from its output P4Info.
::util::Status P4TableMapper::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  const ::p4::config::v1::P4Info& p4_info = config.p4info();
  const std::string& p4_device_config = config.p4_device_config();
  ::util::Status status = ::util::OkStatus();

  // The temporary P4InfoManager verifies the config's p4_info to make sure
  // P4TableMapper doesn't try to handle any invalid P4 objects.
  std::unique_ptr<P4InfoManager> p4_info_manager =
      absl::make_unique<P4InfoManager>(p4_info);
  APPEND_STATUS_IF_ERROR(status, p4_info_manager->InitializeAndVerify());

  // The p4_device_config byte stream in this case is nothing but the
  // serialized version of P4PipelineConfig. Make sure it can be parsed.
  P4PipelineConfig p4_pipeline_config;
  if (!p4_pipeline_config.ParseFromString(p4_device_config)) {
    ::util::Status error =
        MAKE_ERROR(ERR_INTERNAL)
        << "Failed to parse p4_device_config byte stream to P4PipelineConfig.";
    APPEND_STATUS_IF_ERROR(status, error);
  }

  // P4TableMapper can't continue without P4PipelineConfig.
  if (!status.ok()) return status;

  std::unique_ptr<P4ConfigVerifier> p4_config_verifier =
      P4ConfigVerifier::CreateInstance(p4_info, p4_pipeline_config);
  ::util::Status verify_status = ::util::OkStatus();
  if (p4_info_manager_ != nullptr) {
    verify_status = p4_config_verifier->VerifyAndCompare(
        p4_info_manager_->p4_info(), p4_pipeline_config_);
  } else {
    ::p4::config::v1::P4Info empty_p4_info;
    verify_status = p4_config_verifier->VerifyAndCompare(empty_p4_info,
                                                         p4_pipeline_config_);
  }
  APPEND_STATUS_IF_ERROR(status, verify_status);

  return status;
}

::util::Status P4TableMapper::Shutdown() {
  // TODO(unknown): Implement this function if needed.
  return ::util::OkStatus();
}

::util::Status P4TableMapper::MapFlowEntry(
    const ::p4::v1::TableEntry& table_entry, ::p4::v1::Update::Type update_type,
    CommonFlowEntry* flow_entry) const {
  if (flow_entry == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null flow_entry!";
  }

  if (p4_info_manager_ == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unable to map TableEntry without valid P4 configuration";
  }

  flow_entry->Clear();
  ::util::Status status = ::util::OkStatus();

  // The table should be recognized in the P4Info, and it must contain a
  // valid set of match fields and one action.
  int p4_table_id = table_entry.table_id();
  ASSIGN_OR_RETURN(::p4::config::v1::Table table_p4_info,
                   p4_info_manager_->FindTableByID(p4_table_id));
  std::vector<::p4::v1::FieldMatch> all_match_fields;
  RETURN_IF_ERROR(
      PrepareMatchFields(table_p4_info, table_entry, &all_match_fields));
  if (update_type == ::p4::v1::Update::INSERT && !table_entry.has_action()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "P4 TableEntry update has no action";
  }

  APPEND_STATUS_IF_ERROR(
      status, ProcessTableID(table_p4_info, p4_table_id, flow_entry));

  for (const auto& match_field : all_match_fields) {
    APPEND_STATUS_IF_ERROR(
        status, ProcessMatchField(table_p4_info, match_field, flow_entry));
  }

  if (table_entry.has_action()) {
    APPEND_STATUS_IF_ERROR(
        status,
        ProcessTableAction(table_p4_info, table_entry.action(), flow_entry));
  }

  flow_entry->set_priority(table_entry.priority());
  flow_entry->set_controller_metadata(table_entry.controller_metadata());
  return status;
}

::util::Status P4TableMapper::MapActionProfileMember(
    const ::p4::v1::ActionProfileMember& member,
    MappedAction* mapped_action) const {
  if (mapped_action == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null mapped_action!";
  }

  mapped_action->Clear();
  if (p4_info_manager_ == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Unable to map ActionProfileMember "
                                    << "without valid P4 configuration";
  }
  ASSIGN_OR_RETURN(
      const ::p4::config::v1::ActionProfile& profile_p4_info,
      p4_info_manager_->FindActionProfileByID(member.action_profile_id()));

  return ProcessProfileActionFunction(profile_p4_info, member.action(),
                                      mapped_action);
}

::util::Status P4TableMapper::MapActionProfileGroup(
    const ::p4::v1::ActionProfileGroup& group,
    MappedAction* mapped_action) const {
  if (mapped_action == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null mapped_action!";
  }

  mapped_action->Clear();
  if (p4_info_manager_ == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unable to map ActionProfileGroup without valid P4 configuration";
  }
  ASSIGN_OR_RETURN(
      const ::p4::config::v1::ActionProfile& unused_profile_p4_info,
      p4_info_manager_->FindActionProfileByID(group.action_profile_id()));
  mapped_action->set_type(P4_ACTION_TYPE_PROFILE_GROUP_ID);
  // TODO(unknown): Refactor the code in P4InfoManager so that you do not
  // need to do tricks like this to work around the "unused var" error.
  VLOG(4) << unused_profile_p4_info.ShortDebugString();

  return ::util::OkStatus();
}

namespace {

// These two functions take an unsigned 64/32 bit integer and encode it as a
// byte stream in network order.
std::string Uint64ToByteStream(uint64 val) {
  uint64 tmp = (htonl(1) == 1)
                   ? val
                   : (static_cast<uint64>(htonl(val)) << 32) | htonl(val >> 32);
  std::string bytes = "";
  bytes.assign(reinterpret_cast<char*>(&tmp), sizeof(uint64));
  // Strip leading zeroes.
  while (bytes.size() > 1 && bytes[0] == '\x00') {
    bytes = bytes.substr(1);
  }
  return bytes;
}

std::string Uint32ToByteStream(uint32 val) {
  uint32 tmp = htonl(val);
  std::string bytes = "";
  bytes.assign(reinterpret_cast<char*>(&tmp), sizeof(uint32));
  // Strip leading zeroes.
  while (bytes.size() > 1 && bytes[0] == '\x00') {
    bytes = bytes.substr(1);
  }
  return bytes;
}

// TODO(unknown): If needed, add extra validation of the unsigned int values to
// to be in range [1, 2^bitwidth -1].
::util::Status DeparseMetadataHelper(
    const MetadataTypeToIdBitwidthMap& metadata_type_to_id_bitwidth_pair,
    const MappedPacketMetadata& mapped_packet_metadata,
    ::p4::v1::PacketMetadata* p4_packet_metadata) {
  auto* p = gtl::FindOrNull(metadata_type_to_id_bitwidth_pair,
                            mapped_packet_metadata.type());
  if (p == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Don't know how to deparse the following mapped metadata: "
           << mapped_packet_metadata.ShortDebugString() << ".";
  }
  uint32 id = p->first;
  int bitwidth = p->second;
  p4_packet_metadata->set_metadata_id(id);
  switch (mapped_packet_metadata.value_case()) {
    case MappedPacketMetadata::kU32: {
      if (bitwidth > 32) {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Incorrect bitwidth for a u32: " << bitwidth
               << ". Mapped metadata: "
               << mapped_packet_metadata.ShortDebugString() << ".";
      }
      p4_packet_metadata->set_value(
          Uint32ToByteStream(mapped_packet_metadata.u32()));
      break;
    }
    case MappedPacketMetadata::kU64: {
      if (bitwidth <= 32 || bitwidth > 64) {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Incorrect bitwidth for a u64: " << bitwidth
               << ". Mapped metadata: "
               << mapped_packet_metadata.ShortDebugString() << ".";
      }
      p4_packet_metadata->set_value(
          Uint64ToByteStream(mapped_packet_metadata.u64()));
      break;
    }
    case MappedPacketMetadata::kB: {
      if (bitwidth <= 64) {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Incorrect bitwidth for a byte stream: " << bitwidth
               << ". Mapped metadata: "
               << mapped_packet_metadata.ShortDebugString() << ".";
      }
      p4_packet_metadata->set_value(mapped_packet_metadata.b());
      break;
    }
    case MappedPacketMetadata::VALUE_NOT_SET:
      VLOG(1) << "Skipping metadata with no data.";
      break;
  }

  return ::util::OkStatus();
}

// TODO(unknown): If needed, add extra validation of the unsigned int values to
// to be in range [1, 2^bitwidth -1].
::util::Status ParseMetadataHelper(
    const MetadataIdToTypeBitwidthMap& metadata_id_to_type_bitwidth_pair,
    const ::p4::v1::PacketMetadata& p4_packet_metadata,
    MappedPacketMetadata* mapped_packet_metadata) {
  auto* p = gtl::FindOrNull(metadata_id_to_type_bitwidth_pair,
                            p4_packet_metadata.metadata_id());
  if (p == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Don't know how to parse the following P4 metadata: "
           << p4_packet_metadata.ShortDebugString() << ".";
  }
  P4FieldType type = p->first;
  int bitwidth = p->second;
  mapped_packet_metadata->set_type(type);
  if (bitwidth <= 32) {
    mapped_packet_metadata->set_u32(
        ByteStreamToUint<uint32>(p4_packet_metadata.value()));
  } else if (bitwidth <= 64) {
    mapped_packet_metadata->set_u64(
        ByteStreamToUint<uint64>(p4_packet_metadata.value()));
  } else {
    mapped_packet_metadata->set_b(p4_packet_metadata.value());
  }

  return ::util::OkStatus();
}

}  // namespace

::util::Status P4TableMapper::DeparsePacketInMetadata(
    const MappedPacketMetadata& mapped_packet_metadata,
    ::p4::v1::PacketMetadata* p4_packet_metadata) const {
  return DeparseMetadataHelper(packetin_metadata_type_to_id_bitwidth_pair_,
                               mapped_packet_metadata, p4_packet_metadata);
}

::util::Status P4TableMapper::ParsePacketOutMetadata(
    const ::p4::v1::PacketMetadata& p4_packet_metadata,
    MappedPacketMetadata* mapped_packet_metadata) const {
  return ParseMetadataHelper(packetout_metadata_id_to_type_bitwidth_pair_,
                             p4_packet_metadata, mapped_packet_metadata);
}

::util::Status P4TableMapper::DeparsePacketOutMetadata(
    const MappedPacketMetadata& mapped_packet_metadata,
    ::p4::v1::PacketMetadata* p4_packet_metadata) const {
  return DeparseMetadataHelper(packetout_metadata_type_to_id_bitwidth_pair_,
                               mapped_packet_metadata, p4_packet_metadata);
}

::util::Status P4TableMapper::ParsePacketInMetadata(
    const ::p4::v1::PacketMetadata& p4_packet_metadata,
    MappedPacketMetadata* mapped_packet_metadata) const {
  return ParseMetadataHelper(packetin_metadata_id_to_type_bitwidth_pair_,
                             p4_packet_metadata, mapped_packet_metadata);
}

::util::Status P4TableMapper::MapMatchField(int table_id, uint32 field_id,
                                            MappedField* mapped_field) const {
  P4FieldConvertKey key = MakeP4FieldConvertKey(table_id, field_id);
  const P4FieldConvertValue* lookup =
      gtl::FindOrNull(field_convert_by_table_, key);
  if (lookup == nullptr) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "Unrecognized field id " << field_id << " from table "
           << PrintP4ObjectID(table_id) << ".";
  }
  *mapped_field = lookup->mapped_field;
  return ::util::OkStatus();
}

::util::Status P4TableMapper::LookupTable(
    int table_id, ::p4::config::v1::Table* table) const {
  ASSIGN_OR_RETURN(*table, p4_info_manager_->FindTableByID(table_id));
  return ::util::OkStatus();
}

void P4TableMapper::EnableStaticTableUpdates() {
  static_table_updates_enabled_ = true;
}

void P4TableMapper::DisableStaticTableUpdates() {
  static_table_updates_enabled_ = false;
}

::util::Status P4TableMapper::HandlePrePushStaticEntryChanges(
    const ::p4::v1::WriteRequest& new_static_config,
    ::p4::v1::WriteRequest* out_request) {
  // This call should work before the first pipeline config is pushed.
  return static_entry_mapper_->HandlePrePushChanges(new_static_config,
                                                    out_request);
}

::util::Status P4TableMapper::HandlePostPushStaticEntryChanges(
    const ::p4::v1::WriteRequest& new_static_config,
    ::p4::v1::WriteRequest* out_request) {
  if (p4_info_manager_ == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unable to handle static entries without valid P4 configuration";
  }
  return static_entry_mapper_->HandlePostPushChanges(new_static_config,
                                                     out_request);
}

TriState P4TableMapper::IsTableStageHidden(int table_id) const {
  auto desc_iter = global_id_table_map_.find(table_id);
  if (desc_iter == global_id_table_map_.end() ||
      !desc_iter->second->has_table_descriptor()) {
    VLOG(1) << "P4 table ID " << table_id << " has no table descriptor.";
    return TRI_STATE_UNKNOWN;
  }
  const auto& table_descriptor = desc_iter->second->table_descriptor();
  if (table_descriptor.pipeline_stage() == P4Annotation::HIDDEN)
    return TRI_STATE_TRUE;
  return TRI_STATE_FALSE;
}

std::unique_ptr<P4TableMapper> P4TableMapper::CreateInstance() {
  return absl::WrapUnique(new P4TableMapper());
}

::util::Status P4TableMapper::AddMapEntryFromPreamble(
    const ::p4::config::v1::Preamble& preamble) {
  std::string name_key = GetMapperNameKey(preamble);
  if (!name_key.empty()) {
    auto iter = p4_pipeline_config_.table_map().find(name_key);
    if (iter != p4_pipeline_config_.table_map().end()) {
      const auto& descriptor = iter->second;
      global_id_table_map_[preamble.id()] = &descriptor;
    } else {
      // TODO(unknown): Objects with no mapping only generate a warning so
      // development can proceed without full mapping data.
      LOG(WARNING) << "No table mapping for P4 object: "
                   << preamble.DebugString();
    }
  } else {
    // TODO(unknown): Missing P4 names are currently just logged;
    // make missing names an error.
    LOG(WARNING) << "Missing P4 object name in preamble: "
                 << preamble.DebugString();
  }

  return ::util::OkStatus();
}

std::string P4TableMapper::GetMapperNameKey(
    const ::p4::config::v1::Preamble& preamble) {
  return preamble.name();
}

::util::Status P4TableMapper::PrepareMatchFields(
    const ::p4::config::v1::Table& table_p4_info,
    const ::p4::v1::TableEntry& table_entry,
    std::vector<::p4::v1::FieldMatch>* all_match_fields) const {
  // An empty set of match fields changes the default action for tables
  // that were not defined with a const default action in the P4 program.
  if (table_entry.match_size() == 0) {
    if (table_p4_info.const_default_action_id() != 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "P4 TableEntry update attempts to change default action "
             << "of table " << table_p4_info.preamble().name()
             << " with a const default action";
    }
    return ::util::OkStatus();
  }

  // Per field validations:
  //  - Every field_id must be non-zero.
  //  - A field_id can appear in a match field at most once.
  std::set<uint32> requested_field_ids;
  for (const auto& match_field : table_entry.match()) {
    if (match_field.field_id() == 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "P4 TableEntry match field has no field_id. "
             << table_entry.ShortDebugString();
    }
    if (!requested_field_ids.insert(match_field.field_id()).second) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "P4 TableEntry update of table "
             << table_p4_info.preamble().name() << " has multiple match field "
             << "entries for field_id " << match_field.field_id() << ". "
             << table_entry.ShortDebugString();
    }
    all_match_fields->push_back(match_field);
  }

  // Any missing fields in the request are added with don't care values below.
  // The P4MatchKey instance in ProcessMatchField ultimately determines whether
  // don't-care/default usage is permissible for each field.
  for (const auto& p4info_match_field : table_p4_info.match_fields()) {
    if (requested_field_ids.find(p4info_match_field.id()) ==
        requested_field_ids.end()) {
      ::p4::v1::FieldMatch dont_care_match;
      dont_care_match.set_field_id(p4info_match_field.id());
      all_match_fields->push_back(dont_care_match);
    }
  }

  return ::util::OkStatus();
}

::util::Status P4TableMapper::ProcessTableID(
    const ::p4::config::v1::Table& table_p4_info, int table_id,
    CommonFlowEntry* flow_entry) const {
  flow_entry->mutable_table_info()->set_id(table_id);
  flow_entry->mutable_table_info()->set_name(table_p4_info.preamble().name());
  *flow_entry->mutable_table_info()->mutable_annotations() =
      table_p4_info.preamble().annotations();

  auto desc_iter = global_id_table_map_.find(table_id);
  if (desc_iter == global_id_table_map_.end()) {
    flow_entry->mutable_table_info()->set_type(P4_TABLE_UNKNOWN);
    return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
           << "P4 table ID " << table_id << " is missing a table descriptor.";
  }

  const auto& table_descriptor = desc_iter->second->table_descriptor();
  RETURN_IF_ERROR(IsTableUpdateAllowed(table_p4_info, table_descriptor));
  // Information from the table descriptor includes the mapped type, mapped
  // pipeline stage, and any internal match fields.
  flow_entry->mutable_table_info()->set_type(table_descriptor.type());
  flow_entry->mutable_table_info()->set_pipeline_stage(
      table_descriptor.pipeline_stage());
  *flow_entry->mutable_fields() = table_descriptor.internal_match_fields();

  return ::util::OkStatus();
}

// If progress advances this far, ProcessMatchField makes every effort to
// produce some output for the field in flow_entry, even if it is just a raw
// copy of an unknown field.
::util::Status P4TableMapper::ProcessMatchField(
    const ::p4::config::v1::Table& table_p4_info,
    const ::p4::v1::FieldMatch& match_field,
    CommonFlowEntry* flow_entry) const {
  ::util::Status status = ::util::OkStatus();

  // This lookup in field_convert_by_table_ accomplishes two things:
  //  1) It confirms that the field is allowed in the table.
  //  2) It indicates how to map the field into the flow_entry output.
  P4FieldConvertKey key = MakeP4FieldConvertKey(table_p4_info, match_field);
  auto convert_iter = field_convert_by_table_.find(key);
  if (convert_iter == field_convert_by_table_.end()) {
    ::util::Status field_error = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                                 << "P4 TableEntry match field ID "
                                 << PrintP4ObjectID(match_field.field_id())
                                 << " is not recognized in table "
                                 << table_p4_info.preamble().name();
    APPEND_STATUS_IF_ERROR(status, field_error);
    return status;  // No way to decode fields that don't go with the table.
  }

  const auto& conversion_value = convert_iter->second;
  const auto& conversion_entry = conversion_value.conversion_entry;
  const auto& conversion_field = conversion_value.mapped_field;

  std::unique_ptr<P4MatchKey> match_key =
      P4MatchKey::CreateInstance(match_field);
  auto mapped_field = flow_entry->add_fields();
  status = match_key->Convert(conversion_entry, conversion_field.bit_width(),
                              mapped_field);
  if (status.ok()) {
    mapped_field->set_type(conversion_field.type());
    mapped_field->set_bit_width(conversion_field.bit_width());
    mapped_field->set_bit_offset(conversion_field.bit_offset());
    mapped_field->set_header_type(conversion_field.header_type());
  } else {
    mapped_field->set_type(P4_FIELD_TYPE_UNKNOWN);
    status = APPEND_ERROR(status)
             << " for match field " << match_field.ShortDebugString()
             << " in table " << table_p4_info.preamble().name();
  }

  return status;
}

::util::Status P4TableMapper::ProcessTableAction(
    const ::p4::config::v1::Table& table_p4_info,
    const ::p4::v1::TableAction& table_action,
    CommonFlowEntry* flow_entry) const {
  ::util::Status status = ::util::OkStatus();

  // Action profile group and member IDs are easy - the ID just copies
  // directly to the output flow_entry.
  // TODO(unknown): Should there be validation that the actions embedded in
  // the profile are valid for the table in table_p4_info?  This might
  // require a lot of state.  It could potentially be done during the action
  // profile updates instead.
  auto mapped_action = flow_entry->mutable_action();
  switch (table_action.type_case()) {
    case ::p4::v1::TableAction::kAction:
      APPEND_STATUS_IF_ERROR(
          status, ProcessTableActionFunction(
                      table_p4_info, table_action.action(), mapped_action));
      break;
    case ::p4::v1::TableAction::kActionProfileMemberId:
      mapped_action->set_type(P4_ACTION_TYPE_PROFILE_MEMBER_ID);
      mapped_action->set_profile_member_id(
          table_action.action_profile_member_id());
      break;
    case ::p4::v1::TableAction::kActionProfileGroupId:
      mapped_action->set_type(P4_ACTION_TYPE_PROFILE_GROUP_ID);
      mapped_action->set_profile_group_id(
          table_action.action_profile_group_id());
      break;
    case ::p4::v1::TableAction::kActionProfileActionSet:
    case ::p4::v1::TableAction::TYPE_NOT_SET: {
      ::util::Status convert_error =
          MAKE_ERROR(ERR_INVALID_PARAM)
          << "Unrecognized P4 TableEntry action type "
          << table_action.ShortDebugString() << " for table "
          << table_p4_info.preamble().name();
      APPEND_STATUS_IF_ERROR(status, convert_error);
      break;
    }
  }

  return status;
}

// Hands off to the common ProcessActionFunction after doing action validation
// specific to tables.
::util::Status P4TableMapper::ProcessTableActionFunction(
    const ::p4::config::v1::Table& table_p4_info,
    const ::p4::v1::Action& action, MappedAction* mapped_action) const {
  if (action.action_id() == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "P4 TableEntry action has no action_id.";
  }

  // The next function validates that the P4Info and mapping descriptors
  // both recognize this action's ID as a valid action for the table.
  RETURN_IF_ERROR(param_mapper_->IsActionInTableInfo(
      table_p4_info.preamble().id(), action.action_id()));

  ::util::Status status = ProcessActionFunction(action, mapped_action);
  return status;
}

::util::Status P4TableMapper::ProcessProfileActionFunction(
    const ::p4::config::v1::ActionProfile& profile_p4_info,
    const ::p4::v1::Action& action, MappedAction* mapped_action) const {
  if (action.action_id() == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "P4 ActionProfileMember action has no action_id.";
  }

  // The designated action_id should be recognized by every table that
  // shares this action profile.
  for (int table_id : profile_p4_info.table_ids()) {
    RETURN_IF_ERROR(
        param_mapper_->IsActionInTableInfo(table_id, action.action_id()));
  }

  ::util::Status status = ProcessActionFunction(action, mapped_action);
  return status;
}

::util::Status P4TableMapper::ProcessActionFunction(
    const ::p4::v1::Action& action, MappedAction* mapped_action) const {
  ::util::Status status = ::util::OkStatus();
  auto desc_iter = global_id_table_map_.find(action.action_id());
  if (desc_iter != global_id_table_map_.end()) {
    mapped_action->set_type(desc_iter->second->action_descriptor().type());
  } else {
    mapped_action->set_type(P4_ACTION_TYPE_UNKNOWN);
    ::util::Status action_error = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                                  << "P4 action ID in "
                                  << action.ShortDebugString()
                                  << " is unknown or invalid.";
    APPEND_STATUS_IF_ERROR(status, action_error);
    return status;
  }

  // This loop uses param_mapper_ to figure out which header fields are
  // modified by the action's parameters.
  for (const auto& param : action.params()) {
    APPEND_STATUS_IF_ERROR(
        status, param_mapper_->MapActionParam(action.action_id(), param,
                                              mapped_action));
  }

  // Some actions assign constants or use them to call other actions.
  APPEND_STATUS_IF_ERROR(status, param_mapper_->MapActionConstants(
                                     action.action_id(), mapped_action));

  // The action descriptor identifies any additional primitives of this action
  // that don't expect parameters.
  const auto& action_descriptor = desc_iter->second->action_descriptor();
  for (int p = 0; p < action_descriptor.primitive_ops_size(); ++p) {
    const P4ActionOp primitive = action_descriptor.primitive_ops(p);
    mapped_action->mutable_function()->add_primitives()->set_op_code(primitive);
  }

  // The action descriptor's color_actions contain instructions that are
  // conditional based on meter color.
  for (const auto& color_action : action_descriptor.color_actions()) {
    for (const auto& color_op : color_action.ops()) {
      for (int p = 0; p < color_op.primitives_size(); ++p) {
        P4ActionOp primitive = color_op.primitives(p);
        P4ActionFunction::P4ActionPrimitive* mapped_primitive =
            mapped_action->mutable_function()->add_primitives();
        mapped_primitive->set_op_code(primitive);
        for (int c = 0; c < color_action.colors_size(); ++c) {
          mapped_primitive->add_meter_colors(color_action.colors(c));
        }
      }

      // TODO(unknown): Complete deprecation of destination_field_names.
      if (color_op.destination_field_names_size() != 0 ||
          !color_op.destination_field_name().empty()) {
        // TODO(unknown): All of the existing P4 roles have color-qualified
        // action primitives only. Add support here if this changes.
        LOG(WARNING) << "Meter color action has unexpected destination field "
                     << "assignments: " << color_op.ShortDebugString();
      }
    }
  }

  return status;
}

::util::Status P4TableMapper::IsTableUpdateAllowed(
    const ::p4::config::v1::Table& table_p4_info,
    const P4TableDescriptor& descriptor) const {
  // The static_table_updates_enabled_ flag qualifies updates to tables with
  // static entries, regardless of whether they are hidden.
  if (descriptor.has_static_entries()) {
    if (!static_table_updates_enabled_) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Updates to P4 table " << table_p4_info.preamble().name()
             << " with static entries are not allowed";
    }
    return ::util::OkStatus();
  }

  // Updates to hidden non-static tables are never expected.  At first glance,
  // a hidden table without static entries seems like a non-viable use case,
  // but perhaps there will be a need for a hidden table with nothing but
  // a const default action.
  // TODO(unknown): Maybe p4c should detect and reject this case.
  if (descriptor.pipeline_stage() == P4Annotation::HIDDEN) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Updates to hidden P4 table " << table_p4_info.preamble().name()
           << " are not allowed";
  }

  return ::util::OkStatus();
}

void P4TableMapper::ClearMaps() {
  global_id_table_map_.clear();
  field_convert_by_table_.clear();
  packetin_metadata_type_to_id_bitwidth_pair_.clear();
  packetin_metadata_id_to_type_bitwidth_pair_.clear();
  packetout_metadata_type_to_id_bitwidth_pair_.clear();
  packetout_metadata_id_to_type_bitwidth_pair_.clear();
  param_mapper_.reset(nullptr);
}

// P4ActionParamMapper implementation starts here.
P4TableMapper::P4ActionParamMapper::P4ActionParamMapper(
    const P4InfoManager& p4_info_manager,
    const P4GlobalIDTableMap& p4_global_table_map,
    const P4PipelineConfig& p4c_pipeline_cfg)
    : p4_info_manager_(p4_info_manager),
      p4_global_table_map_(p4_global_table_map),
      p4_pipeline_config_(p4c_pipeline_cfg) {}

::util::Status P4TableMapper::P4ActionParamMapper::AddAction(int table_id,
                                                             int action_id) {
  // The action_id should have P4Info and a p4_global_table_map_ entry.
  ::p4::config::v1::Action action_info;
  ASSIGN_OR_RETURN(action_info, p4_info_manager_.FindActionByID(action_id));
  auto iter = p4_global_table_map_.find(action_id);
  if (iter == p4_global_table_map_.end()) {
    return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
           << "P4 action ID " << PrintP4ObjectID(action_id)
           << " has no action descriptor for table mapping";
  }
  const auto& action_descriptor = iter->second->action_descriptor();
  valid_table_actions_.insert(std::make_pair(table_id, action_id));

  // Each parameter needs to have mapping data setup for processing the
  // parameter when it is referenced by a table or action profile update.
  // The data comes from the action parameter's P4Info and the field descriptor
  // for any header fields affected by modify_field primitives.
  for (const auto& param_info : action_info.params()) {
    auto desc_status =
        FindParameterDescriptor(param_info.name(), action_descriptor);
    if (!desc_status.ok()) continue;  // TODO(unknown): Append an error.
    auto param_descriptor = desc_status.ValueOrDie();
    P4ActionParamEntry param_entry;
    param_entry.bit_width = param_info.bitwidth();
    param_entry.param_descriptor = param_descriptor;
    AddAssignedFields(&param_entry)
        .IgnoreError();  // TODO(unknown): Check status.
    const auto param_key = std::make_pair(action_id, param_info.id());
    action_param_map_[param_key] = param_entry;
  }

  // A few actions do constant-value assignments instead of parameter-based
  // assignments.  This loop sets up mapping data for these cases.
  P4ActionConstants constant_descriptors;
  for (const auto& param_descriptor : action_descriptor.assignments()) {
    if (param_descriptor.assigned_value().source_value_case() ==
        P4AssignSourceValue::kConstantParam) {
      P4ActionParamEntry entry;
      entry.bit_width = param_descriptor.assigned_value().bit_width();
      if (entry.bit_width == 0) {
        entry.bit_width = FLAGS_p4c_constant_bitwidth;
        LOG(WARNING) << "Using default bit width (" << entry.bit_width
                     << ") for constant assignment in P4 action ID "
                     << PrintP4ObjectID(action_id);
      }
      entry.param_descriptor = &param_descriptor;
      AddAssignedFields(&entry).IgnoreError();  // TODO(unknown): Check status.
      constant_descriptors.push_back(entry);
    }
  }
  if (!constant_descriptors.empty()) {
    action_constant_map_.insert(
        std::make_pair(action_id, constant_descriptors));
  }

  return ::util::OkStatus();
}

::util::Status P4TableMapper::P4ActionParamMapper::MapActionParam(
    int action_id, const ::p4::v1::Action::Param& param,
    MappedAction* mapped_action) const {
  ::util::Status status = ::util::OkStatus();

  // The entry from the action_param_map_ has information to map the parameter
  // to mapped_action output.  The output consists of a list of modified
  // header fields and/or a sequence of action primitives to execute.
  const auto param_key = std::make_pair(action_id, param.param_id());
  auto iter = action_param_map_.find(param_key);
  if (iter != action_param_map_.end()) {
    const auto& param_map_entry = iter->second;
    P4ActionFunction::P4ActionFields param_value;
    ConvertParamValue(param, param_map_entry.bit_width, &param_value);
    MapActionAssignment(param_map_entry, param_value, mapped_action);
  } else {
    ::util::Status param_status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                                  << "P4 action parameter "
                                  << param.ShortDebugString()
                                  << " has no mapping descriptor or is not"
                                  << " a recognized parameter for action ID "
                                  << PrintP4ObjectID(action_id);
    APPEND_STATUS_IF_ERROR(status, param_status);
  }

  return status;
}

::util::Status P4TableMapper::P4ActionParamMapper::MapActionConstants(
    int action_id, MappedAction* mapped_action) const {
  // A failure to find the action_id means the action does no constant
  // assignments.
  auto iter = action_constant_map_.find(action_id);
  if (iter != action_constant_map_.end()) {
    const auto& param_map_list = iter->second;
    for (const auto& param_map_entry : param_map_list) {
      P4ActionFunction::P4ActionFields constant_value;
      const uint64 constant_param =
          param_map_entry.param_descriptor->assigned_value().constant_param();
      if (param_map_entry.bit_width <= 32) {
        constant_value.set_u32(constant_param);
      } else if (param_map_entry.bit_width <= 64) {
        constant_value.set_u64(constant_param);
      } else {
        return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
               << "P4 action ID " << PrintP4ObjectID(action_id)
               << " constant bit width " << param_map_entry.bit_width
               << " exceeds maximum size (64)";
      }
      MapActionAssignment(param_map_entry, constant_value, mapped_action);
    }
  }

  return ::util::OkStatus();
}

::util::Status P4TableMapper::P4ActionParamMapper::IsActionInTableInfo(
    int table_id, int action_id) const {
  if (valid_table_actions_.find(std::make_pair(table_id, action_id)) ==
      valid_table_actions_.end()) {
    return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
           << "P4 action ID " << PrintP4ObjectID(action_id)
           << " is not a recognized action for table ID "
           << PrintP4ObjectID(table_id);
  }

  return ::util::OkStatus();
}

::util::Status P4TableMapper::P4ActionParamMapper::AddAssignedFields(
    P4ActionParamEntry* param_entry) const {
  // This loop finds the header-field type for any field that this action
  // modifies.
  // TODO(unknown): Should this enforce the same bitwidth in action parameter
  // and the header field info?  This could become a problem for constant
  // assignments, which are currently treated as 64 bits.
  CHECK_RETURN_IF_FALSE(param_entry->param_descriptor != nullptr);

  // TODO(teverman): Complete deprecation of destination_field_names.
  std::string field_name =
      param_entry->param_descriptor->destination_field_name();
  if (field_name.empty()) {
    if (param_entry->param_descriptor->destination_field_names_size() != 0) {
      field_name = param_entry->param_descriptor->destination_field_names(0);
    }
  }

  if (!field_name.empty()) {
    auto field_desc_iter = p4_pipeline_config_.table_map().find(field_name);
    if (field_desc_iter != p4_pipeline_config_.table_map().end()) {
      const auto& field_descriptor = field_desc_iter->second.field_descriptor();
      param_entry->field_types.push_back(field_descriptor.type());
    } else {
      // TODO(unknown): Append an error.
    }
  }

  return ::util::OkStatus();
}

void P4TableMapper::P4ActionParamMapper::MapActionAssignment(
    const P4ActionParamEntry& param_map_entry,
    const P4ActionFunction::P4ActionFields& param_value,
    MappedAction* mapped_action) {
  auto action_function = mapped_action->mutable_function();
  for (auto field_type : param_map_entry.field_types) {
    auto modify_field = action_function->add_modify_fields();
    *modify_field = param_value;
    modify_field->set_type(field_type);
  }
  for (int op = 0; op < param_map_entry.param_descriptor->primitives_size();
       ++op) {
    // TODO(unknown): Which primitives need param_value?
    auto primitive = action_function->add_primitives();
    primitive->set_op_code(param_map_entry.param_descriptor->primitives(op));
  }
}

::util::StatusOr<const P4ActionDescriptor::P4ActionInstructions*>
P4TableMapper::P4ActionParamMapper::FindParameterDescriptor(
    const std::string& param_name,
    const P4ActionDescriptor& action_descriptor) {
  for (const auto& param_descriptor : action_descriptor.assignments()) {
    // Condition below skips assignments with constant values.
    if (param_descriptor.assigned_value().source_value_case() ==
        P4AssignSourceValue::kParameterName) {
      if (param_name == param_descriptor.assigned_value().parameter_name())
        return &param_descriptor;
    }
  }

  return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
         << "P4 action parameter " << param_name << " does not appear in "
         << "action descriptor " << action_descriptor.ShortDebugString();
}

void P4TableMapper::P4ActionParamMapper::ConvertParamValue(
    const ::p4::v1::Action::Param& param, int bit_width,
    P4ActionFunction::P4ActionFields* value) {
  if (bit_width <= 32)
    value->set_u32(ByteStreamToUint<uint32>(param.value()));
  else if (bit_width <= 64)
    value->set_u64(ByteStreamToUint<uint64>(param.value()));
  else
    value->set_b(param.value());
}

}  // namespace hal
}  // namespace stratum
