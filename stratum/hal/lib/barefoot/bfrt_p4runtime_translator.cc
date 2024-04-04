// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_p4runtime_translator.h"

#include "gflags/gflags.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status BfrtP4RuntimeTranslator::PushChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  ::absl::WriterMutexLock l(&lock_);
  // Port mapping for P4Runtime translation.
  singleton_port_to_sdk_port_.clear();
  sdk_port_to_singleton_port_.clear();
  // Initialize with special ports.
  singleton_port_to_sdk_port_[kSdnUnspecifiedPortId] = 0;
  ASSIGN_OR_RETURN(const auto& cpu_sdk_port,
                   bf_sde_interface_->GetPcieCpuPort(device_id_));
  singleton_port_to_sdk_port_[kSdnCpuPortId] = cpu_sdk_port;
  sdk_port_to_singleton_port_[cpu_sdk_port] = kSdnCpuPortId;
  for (int pipe = 0; pipe < kTnaMaxNumPipes; pipe++) {
    uint32 sdk_port = kTnaRecirculationPortBase | (pipe << 7);
    uint32 sdn_port = kSdnTnaRecirculationPortBase + pipe;
    singleton_port_to_sdk_port_[sdn_port] = sdk_port;
    sdk_port_to_singleton_port_[sdk_port] = sdn_port;
  }

  for (const auto& singleton_port : config.singleton_ports()) {
    uint32 singleton_port_id = singleton_port.id();
    PortKey singleton_port_key(singleton_port.slot(), singleton_port.port(),
                               singleton_port.channel());
    ASSIGN_OR_RETURN(uint32 sdk_port_id,
                     bf_sde_interface_->GetPortIdFromPortKey(
                         device_id_, singleton_port_key));
    singleton_port_to_sdk_port_[singleton_port_id] = sdk_port_id;
    sdk_port_to_singleton_port_[sdk_port_id] = singleton_port_id;
  }

  return ::util::OkStatus();
}

::util::Status BfrtP4RuntimeTranslator::PushForwardingPipelineConfig(
    const ::p4::config::v1::P4Info& p4info) {
  ::absl::WriterMutexLock l(&lock_);
  // Enable P4Runtime translation when user define a new type with
  // p4runtime_translation and user enabled it when starting the Stratum.
  if (!translation_enabled_ || !p4info.has_type_info()) {
    pipeline_require_translation_ = false;
    return ::util::OkStatus();
  }

  // First, store types that need to be translated (will check the type_name
  // later).
  absl::flat_hash_map<std::string, std::string> type_name_to_uri;
  absl::flat_hash_map<std::string, int32> type_name_to_bit_width;
  for (const auto& new_type : p4info.type_info().new_types()) {
    const auto& type_name = new_type.first;
    const auto& value = new_type.second;
    if (value.representation_case() ==
        ::p4::config::v1::P4NewTypeSpec::kTranslatedType) {
      pipeline_require_translation_ = true;
      // TODO(Yi Tseng): Verify URI string
      type_name_to_uri[type_name] = value.translated_type().uri();
      if (value.translated_type().sdn_type_case() ==
          ::p4::config::v1::P4NewTypeTranslation::kSdnBitwidth) {
        type_name_to_bit_width[type_name] =
            value.translated_type().sdn_bitwidth();
      } else {
        // TODO(Yi Tseng): support SDN String translation.
        return MAKE_ERROR(ERR_UNIMPLEMENTED)
               << "Unsupported SDN type: "
               << value.translated_type().sdn_type_case();
      }
    }
  }

  // Second, cache all P4Info ID to URI/Bit width mapping
  // Types that support P4Runtime translation:
  // Table.MatchField, Action.Param, ControllerPacketMetadata.Metadata
  // Counter, Meter, Register (index)
  absl::flat_hash_map<uint32, absl::flat_hash_map<uint32, std::string>>
      table_to_field_to_type_uri;
  absl::flat_hash_map<uint32, absl::flat_hash_map<uint32, std::string>>
      action_to_param_to_type_uri;
  absl::flat_hash_map<uint32, std::string> packet_in_meta_to_type_uri;
  absl::flat_hash_map<uint32, std::string> packet_out_meta_to_type_uri;
  absl::flat_hash_map<uint32, std::string> counter_to_type_uri;
  absl::flat_hash_map<uint32, std::string> meter_to_type_uri;
  absl::flat_hash_map<uint32, std::string> register_to_type_uri;
  absl::flat_hash_map<uint32, absl::flat_hash_map<uint32, int32>>
      table_to_field_to_bit_width;
  absl::flat_hash_map<uint32, absl::flat_hash_map<uint32, int32>>
      action_to_param_to_bit_width;
  absl::flat_hash_map<uint32, int32> packet_in_meta_to_bit_width;
  absl::flat_hash_map<uint32, int32> packet_out_meta_to_bit_width;

  for (const auto& table : p4info.tables()) {
    for (const auto& match_field : table.match_fields()) {
      if (match_field.has_type_name()) {
        const auto& type_name = match_field.type_name().name();
        const auto& table_id = table.preamble().id();
        const auto& match_field_id = match_field.id();
        std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
        if (uri) {
          RET_CHECK(kUriToBitWidth.contains(*uri));
          table_to_field_to_type_uri[table_id][match_field_id] = *uri;
        }
        int32* bit_width = gtl::FindOrNull(type_name_to_bit_width, type_name);
        if (bit_width) {
          table_to_field_to_bit_width[table_id][match_field_id] = *bit_width;
        }
      }
    }
  }
  for (const auto& action : p4info.actions()) {
    for (const auto& param : action.params()) {
      if (param.has_type_name()) {
        const auto& type_name = param.type_name().name();
        const auto& action_id = action.preamble().id();
        const auto& param_id = param.id();
        std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
        if (uri) {
          action_to_param_to_type_uri[action_id][param_id] = *uri;
        }
        int32* bit_width = gtl::FindOrNull(type_name_to_bit_width, type_name);
        if (bit_width) {
          action_to_param_to_bit_width[action_id][param_id] = *bit_width;
        }
      }
    }
  }
  for (const auto& pkt_md : p4info.controller_packet_metadata()) {
    const auto& ctrl_hdr_name = pkt_md.preamble().name();
    absl::flat_hash_map<uint32, std::string>* meta_to_type_uri;
    absl::flat_hash_map<uint32, int32>* meta_to_bit_width;
    if (ctrl_hdr_name == kIngressMetadataPreambleName) {
      meta_to_type_uri = &packet_in_meta_to_type_uri;
      meta_to_bit_width = &packet_in_meta_to_bit_width;
    } else if (ctrl_hdr_name == kEgressMetadataPreambleName) {
      meta_to_type_uri = &packet_out_meta_to_type_uri;
      meta_to_bit_width = &packet_out_meta_to_bit_width;
    } else {
      return MAKE_ERROR(ERR_UNIMPLEMENTED)
             << "Unsupported controller header " << ctrl_hdr_name;
    }
    for (const auto& metadata : pkt_md.metadata()) {
      if (metadata.has_type_name()) {
        const auto& type_name = metadata.type_name().name();
        const auto& md_id = metadata.id();
        std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
        if (uri) {
          meta_to_type_uri->emplace(md_id, *uri);
        }
        int32* bit_width = gtl::FindOrNull(type_name_to_bit_width, type_name);
        if (bit_width) {
          meta_to_bit_width->emplace(md_id, *bit_width);
        }
      }
    }
  }
  for (const auto& counter : p4info.counters()) {
    if (counter.has_index_type_name()) {
      const auto& type_name = counter.index_type_name().name();
      const auto& counter_id = counter.preamble().id();
      std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
      if (uri) {
        counter_to_type_uri[counter_id] = *uri;
      }
    }
  }
  for (const auto& meter : p4info.meters()) {
    if (meter.has_index_type_name()) {
      const auto& type_name = meter.index_type_name().name();
      const auto& meter_id = meter.preamble().id();
      std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
      if (uri) {
        meter_to_type_uri[meter_id] = *uri;
      }
    }
  }

  for (const auto& reg : p4info.registers()) {
    if (reg.has_index_type_name()) {
      const auto& type_name = reg.index_type_name().name();
      const auto& register_id = reg.preamble().id();
      std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
      if (uri) {
        register_to_type_uri[register_id] = *uri;
      }
    }
  }
  table_to_field_to_type_uri_ = table_to_field_to_type_uri;
  action_to_param_to_type_uri_ = action_to_param_to_type_uri;
  packet_in_meta_to_type_uri_ = packet_in_meta_to_type_uri;
  packet_out_meta_to_type_uri_ = packet_out_meta_to_type_uri;
  counter_to_type_uri_ = counter_to_type_uri;
  meter_to_type_uri_ = meter_to_type_uri;
  register_to_type_uri_ = register_to_type_uri;
  table_to_field_to_bit_width_ = table_to_field_to_bit_width;
  action_to_param_to_bit_width_ = action_to_param_to_bit_width;
  packet_in_meta_to_bit_width_ = packet_in_meta_to_bit_width;
  packet_out_meta_to_bit_width_ = packet_out_meta_to_bit_width;
  pipeline_require_translation_ = true;
  return ::util::OkStatus();
}

::util::StatusOr<::p4::v1::TableEntry>
BfrtP4RuntimeTranslator::TranslateTableEntry(const ::p4::v1::TableEntry& entry,
                                             bool to_sdk) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return entry;
  }
  return TranslateTableEntryInternal(entry, to_sdk);
}

::util::StatusOr<::p4::v1::TableEntry>
BfrtP4RuntimeTranslator::TranslateTableEntryInternal(
    const ::p4::v1::TableEntry& entry, bool to_sdk) {
  ::p4::v1::TableEntry translated_entry(entry);
  const auto& table_id = translated_entry.table_id();
  if (table_to_field_to_type_uri_.contains(table_id) &&
      table_to_field_to_bit_width_.contains(table_id)) {
    for (::p4::v1::FieldMatch& field_match :
         *translated_entry.mutable_match()) {
      const auto& field_id = field_match.field_id();
      std::string* uri =
          gtl::FindOrNull(table_to_field_to_type_uri_[table_id], field_id);
      if (!uri) {
        continue;
      }
      int32 from_bit_width = 0;
      int32 to_bit_width = 0;
      if (to_sdk) {
        from_bit_width = gtl::FindWithDefault(
            table_to_field_to_bit_width_[table_id], field_id, 0);
        to_bit_width = gtl::FindWithDefault(kUriToBitWidth, *uri, 0);
      } else {
        from_bit_width = gtl::FindWithDefault(kUriToBitWidth, *uri, 0);
        to_bit_width = gtl::FindWithDefault(
            table_to_field_to_bit_width_[table_id], field_id, 0);
      }
      if (!from_bit_width || !to_bit_width) {
        continue;
      }
      switch (field_match.field_match_type_case()) {
        case ::p4::v1::FieldMatch::kExact: {
          ASSIGN_OR_RETURN(const std::string& new_val,
                           TranslateValue(field_match.exact().value(), *uri,
                                          to_sdk, to_bit_width));
          field_match.mutable_exact()->set_value(new_val);
          break;
        }
        case ::p4::v1::FieldMatch::kTernary: {
          // We only allow the "exact" type of ternary match, which means
          // all bits from mask must be one.
          RET_CHECK(field_match.ternary().mask() ==
                    AllOnesByteString(from_bit_width));
          // New mask with bit width.
          ASSIGN_OR_RETURN(const std::string& new_val,
                           TranslateValue(field_match.ternary().value(), *uri,
                                          to_sdk, to_bit_width));
          field_match.mutable_ternary()->set_value(new_val);
          field_match.mutable_ternary()->set_mask(
              AllOnesByteString(to_bit_width));
          break;
        }
        case ::p4::v1::FieldMatch::kLpm: {
          // Only accept "exact match" LPM value, which means the prefix
          // length must same as the bit width of the field.
          RET_CHECK(field_match.lpm().prefix_len() == from_bit_width);
          ASSIGN_OR_RETURN(const std::string& new_val,
                           TranslateValue(field_match.lpm().value(), *uri,
                                          to_sdk, to_bit_width));
          field_match.mutable_lpm()->set_value(new_val);
          field_match.mutable_lpm()->set_prefix_len(to_bit_width);
          break;
        }
        case ::p4::v1::FieldMatch::kRange: {
          // Only accept "exact match" range value, which means both low
          // and high value must be the same.
          RET_CHECK(field_match.range().low() == field_match.range().high());
          ASSIGN_OR_RETURN(const std::string& new_val,
                           TranslateValue(field_match.range().low(), *uri,
                                          to_sdk, to_bit_width));
          field_match.mutable_range()->set_low(new_val);
          field_match.mutable_range()->set_high(new_val);
          break;
        }
        case ::p4::v1::FieldMatch::kOptional: {
          ASSIGN_OR_RETURN(const std::string& new_val,
                           TranslateValue(field_match.optional().value(), *uri,
                                          to_sdk, to_bit_width));
          field_match.mutable_optional()->set_value(new_val);
          break;
        }
        default:
          return MAKE_ERROR(ERR_UNIMPLEMENTED)
                 << "Unsupported field match type: "
                 << field_match.ShortDebugString();
      }
    }
  }

  switch (translated_entry.action().type_case()) {
    case ::p4::v1::TableAction::kAction: {
      ASSIGN_OR_RETURN(
          *(translated_entry.mutable_action()->mutable_action()),
          TranslateAction(translated_entry.action().action(), to_sdk));
      break;
    }
    case ::p4::v1::TableAction::kActionProfileActionSet: {
      auto* action_set = translated_entry.mutable_action()
                             ->mutable_action_profile_action_set();
      for (::p4::v1::ActionProfileAction& action_profile_action :
           *action_set->mutable_action_profile_actions()) {
        ASSIGN_OR_RETURN(
            *(action_profile_action.mutable_action()),
            TranslateAction(action_profile_action.action(), to_sdk));
      }
      break;
    }
    default:
      break;
  }
  return translated_entry;
}

::util::StatusOr<::p4::v1::ActionProfileMember>
BfrtP4RuntimeTranslator::TranslateActionProfileMember(
    const ::p4::v1::ActionProfileMember& act_prof_mem, bool to_sdk) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return act_prof_mem;
  }
  ::p4::v1::ActionProfileMember translated_apm;
  translated_apm.CopyFrom(act_prof_mem);
  const auto& action_profile_id = act_prof_mem.action_profile_id();
  const auto& member_id = act_prof_mem.member_id();
  ASSIGN_OR_RETURN(*(translated_apm.mutable_action()),
                   TranslateAction(translated_apm.action(), to_sdk));
  return translated_apm;
}

::util::StatusOr<::p4::v1::MeterEntry>
BfrtP4RuntimeTranslator::TranslateMeterEntry(const ::p4::v1::MeterEntry& entry,
                                             bool to_sdk) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return entry;
  }
  ::p4::v1::MeterEntry translated_entry(entry);
  std::string* uri = gtl::FindOrNull(meter_to_type_uri_, entry.meter_id());
  if (entry.has_index() && uri) {
    ASSIGN_OR_RETURN(*translated_entry.mutable_index(),
                     TranslateIndex(translated_entry.index(), *uri, to_sdk))
  }
  return translated_entry;
}

::util::StatusOr<::p4::v1::DirectMeterEntry>
BfrtP4RuntimeTranslator::TranslateDirectMeterEntry(
    const ::p4::v1::DirectMeterEntry& entry, bool to_sdk) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return entry;
  }
  ::p4::v1::DirectMeterEntry translated_entry(entry);
  ASSIGN_OR_RETURN(*translated_entry.mutable_table_entry(),
                   TranslateTableEntryInternal(entry.table_entry(), to_sdk));
  return translated_entry;
}

::util::StatusOr<::p4::v1::Index> BfrtP4RuntimeTranslator::TranslateIndex(
    const ::p4::v1::Index& index, const std::string& uri, bool to_sdk) {
  int64 index_value = index.index();
  if (uri == kUriTnaPortId) {
    ::p4::v1::Index translated_index;
    if (to_sdk) {
      RET_CHECK(singleton_port_to_sdk_port_.contains(
          static_cast<uint32>(index_value)))
          << "Could not find SDK port for singleton port " << index_value
          << ".";
      translated_index.set_index(
          singleton_port_to_sdk_port_[static_cast<uint32>(index_value)]);
    } else {
      RET_CHECK(sdk_port_to_singleton_port_.contains(
          static_cast<uint32>(index_value)))
          << "Could not find singleton port for sdk port " << index_value
          << ".";
      translated_index.set_index(
          sdk_port_to_singleton_port_[static_cast<uint32>(index_value)]);
    }
    return translated_index;
  } else {
    return MAKE_ERROR(ERR_UNIMPLEMENTED) << "Unsupported URI: " << uri;
  }
}

::util::StatusOr<::p4::v1::CounterEntry>
BfrtP4RuntimeTranslator::TranslateCounterEntry(
    const ::p4::v1::CounterEntry& entry, bool to_sdk) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return entry;
  }
  ::p4::v1::CounterEntry translated_entry(entry);
  std::string* uri = gtl::FindOrNull(counter_to_type_uri_, entry.counter_id());
  if (entry.has_index() && uri) {
    ASSIGN_OR_RETURN(*translated_entry.mutable_index(),
                     TranslateIndex(translated_entry.index(), *uri, to_sdk))
  }
  return translated_entry;
}

::util::StatusOr<::p4::v1::DirectCounterEntry>
BfrtP4RuntimeTranslator::TranslateDirectCounterEntry(
    const ::p4::v1::DirectCounterEntry& entry, bool to_sdk) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return entry;
  }
  ::p4::v1::DirectCounterEntry translated_entry(entry);
  ASSIGN_OR_RETURN(*translated_entry.mutable_table_entry(),
                   TranslateTableEntryInternal(entry.table_entry(), to_sdk));
  return translated_entry;
}

::util::StatusOr<::p4::v1::RegisterEntry>
BfrtP4RuntimeTranslator::TranslateRegisterEntry(
    const ::p4::v1::RegisterEntry& entry, bool to_sdk) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return entry;
  }
  ::p4::v1::RegisterEntry translated_entry(entry);
  std::string* uri =
      gtl::FindOrNull(register_to_type_uri_, entry.register_id());
  if (entry.has_index() && uri) {
    ASSIGN_OR_RETURN(*translated_entry.mutable_index(),
                     TranslateIndex(translated_entry.index(), *uri, to_sdk))
  }
  return translated_entry;
}

::util::StatusOr<::p4::v1::Replica> BfrtP4RuntimeTranslator::TranslateReplica(
    const ::p4::v1::Replica& replica, bool to_sdk) {
  ::p4::v1::Replica translated_replica(replica);
  // Since we know we are always translating the port number, we can simply
  // use the port map here.
  if (to_sdk) {
    RET_CHECK(singleton_port_to_sdk_port_.contains(replica.egress_port()))
        << "Could not find SDK port for singleton port "
        << replica.egress_port() << ".";
    translated_replica.set_egress_port(
        singleton_port_to_sdk_port_[replica.egress_port()]);
  } else {
    RET_CHECK(sdk_port_to_singleton_port_.contains(replica.egress_port()))
        << "Could not find singleton port for sdk port "
        << replica.egress_port() << ".";
    translated_replica.set_egress_port(
        sdk_port_to_singleton_port_[replica.egress_port()]);
  }
  return translated_replica;
}

::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>
BfrtP4RuntimeTranslator::TranslatePacketReplicationEngineEntry(
    const ::p4::v1::PacketReplicationEngineEntry& entry, bool to_sdk) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return entry;
  }
  ::p4::v1::PacketReplicationEngineEntry translated_entry(entry);
  switch (translated_entry.type_case()) {
    case ::p4::v1::PacketReplicationEngineEntry::kMulticastGroupEntry: {
      auto* multicast_group_entry =
          translated_entry.mutable_multicast_group_entry();
      for (::p4::v1::Replica& replica :
           *multicast_group_entry->mutable_replicas()) {
        ASSIGN_OR_RETURN(replica, TranslateReplica(replica, to_sdk));
      }
      break;
    }
    case ::p4::v1::PacketReplicationEngineEntry::kCloneSessionEntry: {
      auto* clone_session_entry =
          translated_entry.mutable_clone_session_entry();
      for (::p4::v1::Replica& replica :
           *clone_session_entry->mutable_replicas()) {
        ASSIGN_OR_RETURN(replica, TranslateReplica(replica, to_sdk));
      }
      break;
    }
    default:
      break;
  }
  return translated_entry;
}

::util::StatusOr<::p4::v1::PacketMetadata>
BfrtP4RuntimeTranslator::TranslatePacketMetadata(
    const p4::v1::PacketMetadata& packet_metadata, const std::string& uri,
    int32 bit_width, bool to_sdk) {
  p4::v1::PacketMetadata translated_packet_metadata(packet_metadata);
  ASSIGN_OR_RETURN(*translated_packet_metadata.mutable_value(),
                   TranslateValue(translated_packet_metadata.value(), uri,
                                  to_sdk, bit_width));
  return translated_packet_metadata;
}

::util::StatusOr<::p4::v1::PacketIn> BfrtP4RuntimeTranslator::TranslatePacketIn(
    const ::p4::v1::PacketIn& packet_in) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return packet_in;
  }
  ::p4::v1::PacketIn translated_packet_in(packet_in);
  for (auto& md : *translated_packet_in.mutable_metadata()) {
    const std::string* uri =
        gtl::FindOrNull(packet_in_meta_to_type_uri_, md.metadata_id());
    const int32* bit_width =
        gtl::FindOrNull(packet_in_meta_to_bit_width_, md.metadata_id());
    if (uri && bit_width) {
      ASSIGN_OR_RETURN(
          md, TranslatePacketMetadata(md, *uri, *bit_width, /*to_sdk=*/false))
    }
  }
  return translated_packet_in;
}

::util::StatusOr<::p4::v1::PacketOut>
BfrtP4RuntimeTranslator::TranslatePacketOut(
    const ::p4::v1::PacketOut& packet_out) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return packet_out;
  }
  ::p4::v1::PacketOut translated_packet_out(packet_out);
  for (auto& md : *translated_packet_out.mutable_metadata()) {
    const std::string* uri =
        gtl::FindOrNull(packet_out_meta_to_type_uri_, md.metadata_id());
    if (!uri) {
      continue;
    }
    const int32* bit_width = gtl::FindOrNull(kUriToBitWidth, *uri);
    if (bit_width) {
      ASSIGN_OR_RETURN(
          md, TranslatePacketMetadata(md, *uri, *bit_width, /*to_sdk=*/true))
    }
  }
  return translated_packet_out;
}

::util::StatusOr<::p4::config::v1::P4Info>
BfrtP4RuntimeTranslator::TranslateP4Info(
    const ::p4::config::v1::P4Info& p4info) {
  if (!translation_enabled_ || !p4info.has_type_info()) {
    return p4info;
  }
  ::p4::config::v1::P4Info translated_p4info(p4info);
  absl::flat_hash_map<std::string, std::string> type_name_to_uri;
  for (const auto& new_type : p4info.type_info().new_types()) {
    const auto& type_name = new_type.first;
    const auto& value = new_type.second;
    if (value.representation_case() ==
        ::p4::config::v1::P4NewTypeSpec::kTranslatedType) {
      type_name_to_uri[type_name] = value.translated_type().uri();
    }
  }
  for (auto& table : *translated_p4info.mutable_tables()) {
    for (auto& match_field : *table.mutable_match_fields()) {
      if (match_field.has_type_name()) {
        std::string* uri =
            gtl::FindOrNull(type_name_to_uri, match_field.type_name().name());
        if (uri) {
          RET_CHECK(kUriToBitWidth.contains(*uri));
          match_field.set_bitwidth(kUriToBitWidth.at(*uri));
        }
        match_field.clear_type_name();
      }
    }
  }
  for (auto& action : *translated_p4info.mutable_actions()) {
    for (auto& param : *action.mutable_params()) {
      if (param.has_type_name()) {
        std::string* uri =
            gtl::FindOrNull(type_name_to_uri, param.type_name().name());
        if (uri) {
          RET_CHECK(kUriToBitWidth.contains(*uri));
          param.set_bitwidth(kUriToBitWidth.at(*uri));
        }
        param.clear_type_name();
      }
    }
  }
  for (auto& pkt_md : *translated_p4info.mutable_controller_packet_metadata()) {
    for (auto& metadata : *pkt_md.mutable_metadata()) {
      if (metadata.has_type_name()) {
        std::string* uri =
            gtl::FindOrNull(type_name_to_uri, metadata.type_name().name());
        if (uri) {
          RET_CHECK(kUriToBitWidth.contains(*uri));
          metadata.set_bitwidth(kUriToBitWidth.at(*uri));
        }
        metadata.clear_type_name();
      }
    }
  }
  for (auto& counter : *translated_p4info.mutable_counters()) {
    if (counter.has_index_type_name()) {
      counter.clear_index_type_name();
    }
  }
  for (auto& meter : *translated_p4info.mutable_meters()) {
    if (meter.has_index_type_name()) {
      meter.clear_index_type_name();
    }
  }
  for (auto& reg : *translated_p4info.mutable_registers()) {
    if (reg.has_index_type_name()) {
      reg.clear_index_type_name();
    }
  }
  translated_p4info.clear_type_info();
  return translated_p4info;
}

::util::StatusOr<::p4::v1::Action> BfrtP4RuntimeTranslator::TranslateAction(
    const ::p4::v1::Action& action, bool to_sdk) {
  ::p4::v1::Action translated_action;
  translated_action.CopyFrom(action);
  const auto& action_id = action.action_id();
  if (action_to_param_to_type_uri_.contains(action_id) &&
      action_to_param_to_bit_width_.contains(action_id)) {
    for (::p4::v1::Action_Param& param : *translated_action.mutable_params()) {
      const auto& param_id = param.param_id();
      std::string* uri =
          gtl::FindOrNull(action_to_param_to_type_uri_[action_id], param_id);
      int32 to_bit_width = 0;
      if (to_sdk && uri) {
        to_bit_width = gtl::FindWithDefault(kUriToBitWidth, *uri, 0);
      } else {
        to_bit_width = gtl::FindWithDefault(
            action_to_param_to_bit_width_[action_id], param_id, 0);
      }
      if (uri && to_bit_width) {
        ASSIGN_OR_RETURN(
            const std::string& new_val,
            TranslateValue(param.value(), *uri, to_sdk, to_bit_width));
        param.set_value(new_val);
      }  // else, we don't modify the value if it doesn't need to be
         // translated.
    }
  }
  return translated_action;
}

::util::StatusOr<std::string> BfrtP4RuntimeTranslator::TranslateValue(
    const std::string& value, const std::string& uri, bool to_sdk,
    int32 bit_width) {
  if (uri == kUriTnaPortId) {
    return TranslateTnaPortId(value, to_sdk, bit_width);
  }
  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "Unknown URI: " << uri;
}

::util::StatusOr<std::string> BfrtP4RuntimeTranslator::TranslateTnaPortId(
    const std::string& value, bool to_sdk, int32 bit_width) {
  // Translate type "tna/PortId_t"
  if (to_sdk) {
    // singleton port id(N-byte) -> singleton port id(uint32) -> sdk port
    // id(uint32) -> sdk port id(1 or 2 bytes)
    const uint32 port_id = ByteStreamToUint<uint32>(value);
    RET_CHECK(singleton_port_to_sdk_port_.contains(port_id))
        << "Could not find SDK port for singleton port " << port_id << ".";
    return Uint32ToByteStream(singleton_port_to_sdk_port_[port_id]);
  } else {
    // sdk port id(1 or 2 bytes) -> sdk port id(uint32) -> singleton port
    // id(uint32)
    // -> singleton port id(N-byte)
    RET_CHECK(value.size() <= NumBitsToNumBytes(kTnaPortIdBitWidth))
        << "Port value " << value << " exceeds maximum bit width";
    const uint32 sdk_port_id = ByteStreamToUint<uint32>(value);
    RET_CHECK(sdk_port_to_singleton_port_.contains(sdk_port_id))
        << "Could not find singleton port for sdk port " << sdk_port_id << ".";
    const uint32 port_id = sdk_port_to_singleton_port_[sdk_port_id];
    std::string port_id_bytes = Uint32ToByteStream(port_id);

    return port_id_bytes;
  }
}
}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
