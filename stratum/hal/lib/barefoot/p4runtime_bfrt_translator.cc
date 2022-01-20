// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/p4runtime_bfrt_translator.h"

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

::util::StatusOr<::p4::v1::WriteRequest>
P4RuntimeBfrtTranslator::TranslateWriteRequest(
    const ::p4::v1::WriteRequest& request) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return request;
  }
  ::p4::v1::WriteRequest translated_request(request);
  for (::p4::v1::Update& update : *translated_request.mutable_updates()) {
    ASSIGN_OR_RETURN(*update.mutable_entity(),
                     TranslateEntity(update.entity(), /*to_sdk=*/true));
  }
  return translated_request;
}

::util::StatusOr<::p4::v1::ReadRequest>
P4RuntimeBfrtTranslator::TranslateReadRequest(
    const ::p4::v1::ReadRequest& request) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return request;
  }
  ::p4::v1::ReadRequest translated_request(request);
  for (::p4::v1::Entity& entity : *translated_request.mutable_entities()) {
    ASSIGN_OR_RETURN(entity, TranslateEntity(entity, /*to_sdk=*/true));
  }
  return translated_request;
}

::util::StatusOr<::p4::v1::ReadResponse>
P4RuntimeBfrtTranslator::TranslateReadResponse(
    const ::p4::v1::ReadResponse& response) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_require_translation_) {
    return response;
  }
  ::p4::v1::ReadResponse translated_response(response);
  for (::p4::v1::Entity& entity : *translated_response.mutable_entities()) {
    ASSIGN_OR_RETURN(entity, TranslateEntity(entity, /*to_sdk=*/false));
  }
  return translated_response;
}

bool P4RuntimeBfrtTranslationWriterWrapper::Write(
    const ::p4::v1::ReadResponse& msg) {
  auto status = p4runtime_bfrt_translator_->TranslateReadResponse(msg);
  if (!status.ok()) {
    return false;
  }
  return writer_->Write(status.ConsumeValueOrDie());
}

::util::Status P4RuntimeBfrtTranslator::PushChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  ::absl::WriterMutexLock l(&lock_);
  // Port mapping for P4Runtime translation.
  singleton_port_to_sdk_port_.clear();
  sdk_port_to_singleton_port_.clear();
  // Initialize with special ports.
  ASSIGN_OR_RETURN(const auto& cpu_sdk_port,
                   bf_sde_interface_->GetPcieCpuPort(device_id_));
  singleton_port_to_sdk_port_[kSdnCpuPortId] = cpu_sdk_port;
  sdk_port_to_singleton_port_[cpu_sdk_port] = kSdnCpuPortId;
  for (int pipe = 0; pipe < 4; pipe++) {
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

::util::Status P4RuntimeBfrtTranslator::PushForwardingPipelineConfig(
    const ::p4::config::v1::P4Info& p4info) {
  ::absl::WriterMutexLock l(&lock_);
  pipeline_require_translation_ = false;
  if (!translation_enabled_) {
    return ::util::OkStatus();
  }
  // Enable P4Runtime translation when user define a new type with
  // p4runtime_translation.
  if (p4info.has_type_info()) {
    // First, store types that need to be translated(will check the type_name
    // later)
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
        }
      }
    }

    // Second, cache all P4Info ID to URI/Bit width mapping
    // Types that support P4Runtime translation:
    // Table.MatchField, Action.Param, ControllerPacketMetadata.Metadata
    // Counter, Meter, Register (index)
    table_to_field_to_type_uri_.clear();
    action_to_param_to_type_uri_.clear();
    ctrl_hdr_to_meta_to_type_uri_.clear();
    counter_to_type_uri_.clear();
    meter_to_type_uri_.clear();
    register_to_type_uri_.clear();
    table_to_field_to_bit_width_.clear();
    action_to_param_to_bit_width_.clear();
    ctrl_hdr_to_meta_to_bit_width_.clear();
    counter_to_bit_width_.clear();
    meter_to_bit_width_.clear();
    register_to_bit_width_.clear();

    for (const auto& table : p4info.tables()) {
      for (const auto& match_field : table.match_fields()) {
        if (match_field.has_type_name()) {
          const auto& type_name = match_field.type_name().name();
          const auto& table_id = table.preamble().id();
          const auto& match_field_id = match_field.id();
          std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
          if (uri) {
            table_to_field_to_type_uri_[table_id][match_field_id] = *uri;
          }
          int32* bit_width = gtl::FindOrNull(type_name_to_bit_width, type_name);
          if (bit_width) {
            table_to_field_to_bit_width_[table_id][match_field_id] = *bit_width;
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
            action_to_param_to_type_uri_[action_id][param_id] = *uri;
          }
          int32* bit_width = gtl::FindOrNull(type_name_to_bit_width, type_name);
          if (bit_width) {
            action_to_param_to_bit_width_[action_id][param_id] = *bit_width;
          }
        }
      }
    }
    for (const auto& pkt_md : p4info.controller_packet_metadata()) {
      for (const auto& metadata : pkt_md.metadata()) {
        if (metadata.has_type_name()) {
          const auto& type_name = metadata.type_name().name();
          const auto& ctrl_hdr_id = pkt_md.preamble().id();
          const auto& md_id = metadata.id();
          std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
          if (uri) {
            ctrl_hdr_to_meta_to_type_uri_[ctrl_hdr_id][md_id] = *uri;
          }
          int32* bit_width = gtl::FindOrNull(type_name_to_bit_width, type_name);
          if (bit_width) {
            ctrl_hdr_to_meta_to_bit_width_[ctrl_hdr_id][md_id] = *bit_width;
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
          counter_to_type_uri_[counter_id] = *uri;
        }
        int32* bit_width = gtl::FindOrNull(type_name_to_bit_width, type_name);
        if (bit_width) {
          counter_to_bit_width_[counter_id] = *bit_width;
        }
      }
    }
    for (const auto& meter : p4info.meters()) {
      if (meter.has_index_type_name()) {
        const auto& type_name = meter.index_type_name().name();
        const auto& meter_id = meter.preamble().id();
        std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
        if (uri) {
          meter_to_type_uri_[meter_id] = *uri;
        }
        int32* bit_width = gtl::FindOrNull(type_name_to_bit_width, type_name);
        if (bit_width) {
          meter_to_bit_width_[meter_id] = *bit_width;
        }
      }
    }
    for (const auto& reg : p4info.registers()) {
      if (reg.has_index_type_name()) {
        const auto& type_name = reg.index_type_name().name();
        const auto& register_id = reg.preamble().id();
        std::string* uri = gtl::FindOrNull(type_name_to_uri, type_name);
        if (uri) {
          register_to_type_uri_[register_id] = *uri;
        }
        int32* bit_width = gtl::FindOrNull(type_name_to_bit_width, type_name);
        if (bit_width) {
          register_to_bit_width_[register_id] = *bit_width;
        }
      }
    }
  }

  return ::util::OkStatus();
}

::util::StatusOr<::p4::v1::Entity> P4RuntimeBfrtTranslator::TranslateEntity(
    const ::p4::v1::Entity& entity, bool to_sdk) {
  ::p4::v1::Entity translated_entity(entity);
  switch (translated_entity.entity_case()) {
    case ::p4::v1::Entity::kTableEntry: {
      ASSIGN_OR_RETURN(
          *translated_entity.mutable_table_entry(),
          TranslateTableEntry(translated_entity.table_entry(), to_sdk));
      break;
    }
    case ::p4::v1::Entity::kActionProfileMember: {
      ASSIGN_OR_RETURN(*translated_entity.mutable_action_profile_member(),
                       TranslateActionProfileMember(
                           translated_entity.action_profile_member(), to_sdk));
      break;
    }
    case ::p4::v1::Entity::kPacketReplicationEngineEntry: {
      ASSIGN_OR_RETURN(
          *translated_entity.mutable_packet_replication_engine_entry(),
          TranslatePacketReplicationEngineEntry(
              translated_entity.packet_replication_engine_entry(), to_sdk));
      break;
    }
    case ::p4::v1::Entity::kDirectCounterEntry: {
      ASSIGN_OR_RETURN(*translated_entity.mutable_direct_counter_entry(),
                       TranslateDirectCounterEntry(
                           translated_entity.direct_counter_entry(), to_sdk));
      break;
    }
    case ::p4::v1::Entity::kCounterEntry: {
      ASSIGN_OR_RETURN(
          *translated_entity.mutable_counter_entry(),
          TranslateCounterEntry(translated_entity.counter_entry(), to_sdk));
      break;
    }
    case ::p4::v1::Entity::kRegisterEntry: {
      ASSIGN_OR_RETURN(
          *translated_entity.mutable_register_entry(),
          TranslateRegisterEntry(translated_entity.register_entry(), to_sdk));
      break;
    }
    case ::p4::v1::Entity::kDirectMeterEntry: {
      ASSIGN_OR_RETURN(*translated_entity.mutable_direct_meter_entry(),
                       TranslateDirectMeterEntry(
                           translated_entity.direct_meter_entry(), to_sdk));
      break;
    }
    case ::p4::v1::Entity::kMeterEntry: {
      ASSIGN_OR_RETURN(
          *translated_entity.mutable_meter_entry(),
          TranslateMeterEntry(translated_entity.meter_entry(), to_sdk));
      break;
    }
    default:
      break;
  }
  return translated_entity;
}

::util::StatusOr<::p4::v1::TableEntry>
P4RuntimeBfrtTranslator::TranslateTableEntry(const ::p4::v1::TableEntry& entry,
                                             bool to_sdk) {
  ::p4::v1::TableEntry translated_entry(entry);
  const auto& table_id = translated_entry.table_id();
  if (table_to_field_to_type_uri_.count(table_id) &&
      table_to_field_to_bit_width_.count(table_id)) {
    for (::p4::v1::FieldMatch& field_match :
         *translated_entry.mutable_match()) {
      const auto& field_id = field_match.field_id();
      std::string* uri =
          gtl::FindOrNull(table_to_field_to_type_uri_[table_id], field_id);

      int32 from_bit_width = 0;
      int32 to_bit_width = 0;
      if (to_sdk && uri) {
        from_bit_width = gtl::FindWithDefault(
            table_to_field_to_bit_width_[table_id], field_id, 0);
        to_bit_width = gtl::FindWithDefault(kUriToBitWidth, *uri, 0);
      } else if (uri) {
        from_bit_width = gtl::FindWithDefault(kUriToBitWidth, *uri, 0);
        to_bit_width = gtl::FindWithDefault(
            table_to_field_to_bit_width_[table_id], field_id, 0);
      }
      if (uri && from_bit_width && to_bit_width) {
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
            std::string all_one = std::string(from_bit_width / 8, '\xff');
            if (from_bit_width % 8) {
              all_one.insert(
                  0, 1, static_cast<char>(1 << ((from_bit_width % 8) - 1)));
            }
            CHECK_RETURN_IF_FALSE(field_match.ternary().mask() == all_one);
            // New mask with bit width.
            if (from_bit_width != to_bit_width) {
              all_one = std::string(to_bit_width / 8, '\xff');
              if (to_bit_width % 8) {
                all_one.insert(0, 1,
                               static_cast<char>(1 << (to_bit_width % 8 - 1)));
              }
            }
            ASSIGN_OR_RETURN(const std::string& new_val,
                             TranslateValue(field_match.ternary().value(), *uri,
                                            to_sdk, to_bit_width));
            field_match.mutable_ternary()->set_value(new_val);
            field_match.mutable_ternary()->set_mask(all_one);
            break;
          }
          case ::p4::v1::FieldMatch::kLpm: {
            // Only accept "exact match" LPM value, which means the prefix
            // length must same as the bit width of the field.
            CHECK_RETURN_IF_FALSE(field_match.lpm().prefix_len() ==
                                  from_bit_width);
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
            CHECK_RETURN_IF_FALSE(field_match.range().low() ==
                                  field_match.range().high());
            ASSIGN_OR_RETURN(const std::string& new_val,
                             TranslateValue(field_match.range().low(), *uri,
                                            to_sdk, to_bit_width));
            field_match.mutable_range()->set_low(new_val);
            field_match.mutable_range()->set_high(new_val);
            break;
          }
          case ::p4::v1::FieldMatch::kOptional: {
            ASSIGN_OR_RETURN(const std::string& new_val,
                             TranslateValue(field_match.optional().value(),
                                            *uri, to_sdk, to_bit_width));
            field_match.mutable_optional()->set_value(new_val);
            break;
          }
          default:
            return MAKE_ERROR(ERR_UNIMPLEMENTED)
                   << "Unsupported field match type: "
                   << field_match.ShortDebugString();
        }
      }  // else, we don't modify the value if it doesn't need to be translated.
    }
  }

  if (translated_entry.action().type_case() == ::p4::v1::TableAction::kAction) {
    ASSIGN_OR_RETURN(
        *(translated_entry.mutable_action()->mutable_action()),
        TranslateAction(translated_entry.action().action(), to_sdk));
  } else if (translated_entry.action().type_case() ==
             ::p4::v1::TableAction::kActionProfileActionSet) {
    auto* action_set =
        translated_entry.mutable_action()->mutable_action_profile_action_set();
    for (::p4::v1::ActionProfileAction& action_profile_action :
         *action_set->mutable_action_profile_actions()) {
      ASSIGN_OR_RETURN(*(action_profile_action.mutable_action()),
                       TranslateAction(action_profile_action.action(), to_sdk));
    }
  }  // else, we don't translate action profile member id or group id.

  return translated_entry;
}

::util::StatusOr<::p4::v1::ActionProfileMember>
P4RuntimeBfrtTranslator::TranslateActionProfileMember(
    const ::p4::v1::ActionProfileMember& act_prof_mem, bool to_sdk) {
  ::p4::v1::ActionProfileMember translated_apm;
  translated_apm.CopyFrom(act_prof_mem);
  const auto& action_profile_id = act_prof_mem.action_profile_id();
  const auto& member_id = act_prof_mem.member_id();
  ASSIGN_OR_RETURN(*(translated_apm.mutable_action()),
                   TranslateAction(translated_apm.action(), to_sdk));
  return translated_apm;
}

::util::StatusOr<::p4::v1::MeterEntry>
P4RuntimeBfrtTranslator::TranslateMeterEntry(const ::p4::v1::MeterEntry& entry,
                                             bool to_sdk) {
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}

::util::StatusOr<::p4::v1::DirectMeterEntry>
P4RuntimeBfrtTranslator::TranslateDirectMeterEntry(
    const ::p4::v1::DirectMeterEntry& entry, bool to_sdk) {
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}

::util::StatusOr<::p4::v1::CounterEntry>
P4RuntimeBfrtTranslator::TranslateCounterEntry(
    const ::p4::v1::CounterEntry& entry, bool to_sdk) {
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}

::util::StatusOr<::p4::v1::DirectCounterEntry>
P4RuntimeBfrtTranslator::TranslateDirectCounterEntry(
    const ::p4::v1::DirectCounterEntry& entry, bool to_sdk) {
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}

::util::StatusOr<::p4::v1::RegisterEntry>
P4RuntimeBfrtTranslator::TranslateRegisterEntry(
    const ::p4::v1::RegisterEntry& entry, bool to_sdk) {
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}

::util::StatusOr<::p4::v1::Replica> P4RuntimeBfrtTranslator::TranslateReplica(
    const ::p4::v1::Replica& replica, bool to_sdk) {
  ::p4::v1::Replica translated_replica(replica);
  // Since we know we are always translating the port number, we can simply
  // use the port map here.
  if (to_sdk) {
    CHECK_RETURN_IF_FALSE(
        singleton_port_to_sdk_port_.count(replica.egress_port()));
    translated_replica.set_egress_port(
        singleton_port_to_sdk_port_[replica.egress_port()]);
  } else {
    CHECK_RETURN_IF_FALSE(
        sdk_port_to_singleton_port_.count(replica.egress_port()));
    translated_replica.set_egress_port(
        sdk_port_to_singleton_port_[replica.egress_port()]);
  }
  return translated_replica;
}

::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>
P4RuntimeBfrtTranslator::TranslatePacketReplicationEngineEntry(
    const ::p4::v1::PacketReplicationEngineEntry& entry, bool to_sdk) {
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

::util::StatusOr<::p4::v1::StreamMessageRequest>
P4RuntimeBfrtTranslator::TranslateStreamMessageRequest(
    const ::p4::v1::StreamMessageRequest& request) {
  // TODO(Yi Tseng): Will support this in another PR.
  return request;
}
::util::StatusOr<::p4::v1::StreamMessageResponse>
P4RuntimeBfrtTranslator::TranslateStreamMessageResponse(
    const ::p4::v1::StreamMessageResponse& response) {
  // TODO(Yi Tseng): Will support this in another PR.
  return response;
}

::util::StatusOr<::p4::v1::Action> P4RuntimeBfrtTranslator::TranslateAction(
    const ::p4::v1::Action& action, bool to_sdk) {
  ::p4::v1::Action translated_action;
  translated_action.CopyFrom(action);
  const auto& action_id = action.action_id();
  if (action_to_param_to_type_uri_.count(action_id) &&
      action_to_param_to_bit_width_.count(action_id)) {
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

::util::StatusOr<std::string> P4RuntimeBfrtTranslator::TranslateValue(
    const std::string& value, const std::string& uri, bool to_sdk,
    int32 bit_width) {
  if (uri.compare(kUriTnaPortId) == 0) {
    return TranslateTnaPortId(value, to_sdk, bit_width);
  }
  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "Unknown URI: " << uri;
}

::util::StatusOr<std::string> P4RuntimeBfrtTranslator::TranslateTnaPortId(
    const std::string& value, bool to_sdk, int32 bit_width) {
  // Translate type "tna/PortId_t"
  if (to_sdk) {
    // singleton port id(N-byte) -> singleton port id(uint32) -> sdk port
    // id(uint32) -> sdk port id(2-byte)
    const uint32 port_id = ByteStreamToUint<uint32>(value);
    CHECK_RETURN_IF_FALSE(singleton_port_to_sdk_port_.count(port_id));
    const uint32 sdk_port_id = singleton_port_to_sdk_port_[port_id];
    std::string sdk_port_id_bytes = P4RuntimeByteStringToPaddedByteString(
        Uint32ToByteStream(sdk_port_id), NumBitsToNumBytes(bit_width));
    return sdk_port_id_bytes;
  } else {
    // sdk port id(2-byte) -> sdk port id(uint32) -> singleton port id(uint32)
    // -> singleton port id(N-byte)
    CHECK_RETURN_IF_FALSE(value.size() ==
                          NumBitsToNumBytes(kTnaPortIdBitWidth));
    const uint32 sdk_port_id = ByteStreamToUint<uint32>(value);
    CHECK_RETURN_IF_FALSE(sdk_port_to_singleton_port_.count(sdk_port_id));
    const uint32 port_id = sdk_port_to_singleton_port_[sdk_port_id];
    std::string port_id_bytes = P4RuntimeByteStringToPaddedByteString(
        Uint32ToByteStream(port_id), NumBitsToNumBytes(bit_width));
    return port_id_bytes;
  }
}
}  // namespace barefoot
}  // namespace hal
}  // namespace stratum