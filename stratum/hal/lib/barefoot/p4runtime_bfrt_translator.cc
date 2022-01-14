// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/p4runtime_bfrt_translator.h"

#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace hal {
namespace barefoot {
bool P4RuntimeBfrtTranslationWriterWrapper::Write(
    const ::p4::v1::ReadResponse& msg) {
  ::p4::v1::ReadResponse translated_msg;
  translated_msg.CopyFrom(msg);

  for (int i = 0; i < translated_msg.entities_size(); i++) {
    auto* entity = translated_msg.mutable_entities()->Mutable(i);
    switch (entity->entity_case()) {
      case ::p4::v1::Entity::kTableEntry: {
        auto status = p4runtime_bfrt_translator_->TranslateTableEntry(
            entity->table_entry(), false);
        if (!status.ok()) {
          return false;
        }
        *entity->mutable_table_entry() = status.ConsumeValueOrDie();
        break;
      }
      case ::p4::v1::Entity::kActionProfileMember: {
        auto status = p4runtime_bfrt_translator_->TranslateActionProfileMember(
            entity->action_profile_member(), false);
        if (!status.ok()) {
          return false;
        }
        *entity->mutable_action_profile_member() = status.ConsumeValueOrDie();
        break;
      }
      case ::p4::v1::Entity::kPacketReplicationEngineEntry: {
        auto status =
            p4runtime_bfrt_translator_->TranslatePacketReplicationEngineEntry(
                entity->packet_replication_engine_entry(), false);
        if (!status.ok()) {
          return false;
        }
        *entity->mutable_packet_replication_engine_entry() =
            status.ConsumeValueOrDie();
        break;
      }
      case ::p4::v1::Entity::kDirectCounterEntry: {
        auto status = p4runtime_bfrt_translator_->TranslateDirectCounterEntry(
            entity->direct_counter_entry(), false);
        if (!status.ok()) {
          return false;
        }
        *entity->mutable_direct_counter_entry() = status.ConsumeValueOrDie();
        break;
      }
      case ::p4::v1::Entity::kCounterEntry: {
        auto status = p4runtime_bfrt_translator_->TranslateCounterEntry(
            entity->counter_entry(), false);
        if (!status.ok()) {
          return false;
        }
        *entity->mutable_counter_entry() = status.ConsumeValueOrDie();
        break;
      }
      case ::p4::v1::Entity::kRegisterEntry: {
        auto status = p4runtime_bfrt_translator_->TranslateRegisterEntry(
            entity->register_entry(), false);
        if (!status.ok()) {
          return false;
        }
        *entity->mutable_register_entry() = status.ConsumeValueOrDie();
        break;
      }
      case ::p4::v1::Entity::kMeterEntry: {
        auto status = p4runtime_bfrt_translator_->TranslateMeterEntry(
            entity->meter_entry(), false);
        if (!status.ok()) {
          return false;
        }
        *entity->mutable_meter_entry() = status.ConsumeValueOrDie();
        break;
      }
      default:
        // Skip entity type that no need to be translated.
        break;
    }
  }

  return writer_->Write(translated_msg);
}

::util::Status P4RuntimeBfrtTranslator::PushChassisConfig(
    const ChassisConfig& config) {
  ::absl::WriterMutexLock l(&lock_);
  // Port mapping for P4Runtime translation.
  singleton_port_to_sdk_port_.clear();
  sdk_port_to_singleton_port_.clear();
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
    const ::p4::v1::ForwardingPipelineConfig& config) {
  ::absl::WriterMutexLock l(&lock_);
  const auto& p4info = config.p4info();

  // Enable P4Runtime translation When user define a new type with
  // p4runtime_translation.
  translation_enabled_ = false;

  if (p4info.has_type_info()) {
    // First, store types that need to be translated(will check the type_name
    // later)
    std::map<std::string, std::string> type_name_to_uri;
    std::map<std::string, int32> type_name_to_bit_width;
    for (const auto& new_type : p4info.type_info().new_types()) {
      const auto& type_name = new_type.first;
      const auto& value = new_type.second;
      if (value.representation_case() ==
          ::p4::config::v1::P4NewTypeSpec::kTranslatedType) {
        translation_enabled_ = true;
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

::util::StatusOr<::p4::v1::TableEntry>
P4RuntimeBfrtTranslator::TranslateTableEntry(const ::p4::v1::TableEntry& entry,
                                             const bool& to_sdk) {
  ::absl::ReaderMutexLock l(&lock_);
  ::p4::v1::TableEntry translated_entry;
  translated_entry.CopyFrom(entry);

  const auto& table_id = translated_entry.table_id();
  if (table_to_field_to_type_uri_.count(table_id) &&
      table_to_field_to_bit_width_.count(table_id)) {
    for (int i = 0; i < translated_entry.match_size(); i++) {
      auto* field_match = translated_entry.mutable_match()->Mutable(i);
      const auto& field_id = field_match->field_id();
      std::string* uri =
          gtl::FindOrNull(table_to_field_to_type_uri_[table_id], field_id);
      int32* bit_width =
          gtl::FindOrNull(table_to_field_to_bit_width_[table_id], field_id);
      if (uri && bit_width) {
        switch (field_match->field_match_type_case()) {
          case ::p4::v1::FieldMatch::kExact: {
            ASSIGN_OR_RETURN(const std::string& new_val,
                             TranslateValue(field_match->exact().value(), *uri,
                                            to_sdk, *bit_width));
            field_match->mutable_exact()->set_value(new_val);
            break;
          }
          case ::p4::v1::FieldMatch::kTernary: {
            ASSIGN_OR_RETURN(const std::string& new_val,
                             TranslateValue(field_match->ternary().value(),
                                            *uri, to_sdk, *bit_width));
            field_match->mutable_ternary()->set_value(new_val);
            break;
          }
          case ::p4::v1::FieldMatch::kLpm: {
            ASSIGN_OR_RETURN(const std::string& new_val,
                             TranslateValue(field_match->lpm().value(), *uri,
                                            to_sdk, *bit_width));
            field_match->mutable_lpm()->set_value(new_val);
            break;
          }
          case ::p4::v1::FieldMatch::kRange: {
            ASSIGN_OR_RETURN(const std::string& new_low_val,
                             TranslateValue(field_match->range().low(), *uri,
                                            to_sdk, *bit_width));
            ASSIGN_OR_RETURN(const std::string& new_high_val,
                             TranslateValue(field_match->range().high(), *uri,
                                            to_sdk, *bit_width));
            field_match->mutable_range()->set_low(new_low_val);
            field_match->mutable_range()->set_high(new_high_val);
            break;
          }
          case ::p4::v1::FieldMatch::kOptional: {
            ASSIGN_OR_RETURN(const std::string& new_val,
                             TranslateValue(field_match->optional().value(),
                                            *uri, to_sdk, *bit_width));
            field_match->mutable_optional()->set_value(new_val);
            break;
          }
          default:
            RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unsupported field match type: "
                                            << field_match->ShortDebugString();
        }
      }  // else, we don't modify the value if it doesn't need to be translated.
    }
  }

  if (translated_entry.action().type_case() == ::p4::v1::TableAction::kAction) {
    auto* action = translated_entry.mutable_action()->mutable_action();
    const auto& action_id = action->action_id();
    if (action_to_param_to_type_uri_.count(action_id) &&
        action_to_param_to_bit_width_.count(action_id)) {
      for (int i = 0; i < action->params_size(); i++) {
        auto* param = action->mutable_params()->Mutable(i);
        const auto& param_id = param->param_id();
        std::string* uri =
            gtl::FindOrNull(action_to_param_to_type_uri_[action_id], param_id);
        int32* bit_width =
            gtl::FindOrNull(action_to_param_to_bit_width_[action_id], param_id);
        if (uri && bit_width) {
          ASSIGN_OR_RETURN(
              const std::string& new_val,
              TranslateValue(param->value(), *uri, to_sdk, *bit_width));
          param->set_value(new_val);
        }  // else, we don't modify the value if it doesn't need to be
           // translated.
      }
    }
  } else if (translated_entry.action().type_case() ==
             ::p4::v1::TableAction::kActionProfileActionSet) {
    auto* action_profile_action_set =
        translated_entry.mutable_action()->mutable_action_profile_action_set();
    for (int i = 0;
         i < action_profile_action_set->action_profile_actions_size(); i++) {
      auto* action_profile_action =
          action_profile_action_set->mutable_action_profile_actions()->Mutable(
              i);
      auto* action = action_profile_action->mutable_action();
      const auto& action_id = action->action_id();
      if (action_to_param_to_type_uri_.count(action_id) &&
          action_to_param_to_bit_width_.count(action_id)) {
        for (int j = 0; j < action->params_size(); j++) {
          auto* param = action->mutable_params()->Mutable(j);
          const auto& param_id = param->param_id();
          std::string* uri = gtl::FindOrNull(
              action_to_param_to_type_uri_[action_id], param_id);
          int32* bit_width = gtl::FindOrNull(
              action_to_param_to_bit_width_[action_id], param_id);
          if (uri && bit_width) {
            ASSIGN_OR_RETURN(
                const std::string& new_val,
                TranslateValue(param->value(), *uri, to_sdk, *bit_width));
            param->set_value(new_val);
          }  // else, we don't modify the value if it doesn't need to be
             // translated.
        }
      }
    }
  }

  return translated_entry;
}

::util::StatusOr<::p4::v1::ActionProfileMember>
P4RuntimeBfrtTranslator::TranslateActionProfileMember(
    const ::p4::v1::ActionProfileMember& action_prof_mem, const bool& to_sdk) {
  ::absl::ReaderMutexLock l(&lock_);
  // TODO(Yi Tseng): Will support this in another PR.
  return action_prof_mem;
}
::util::StatusOr<::p4::v1::MeterEntry>
P4RuntimeBfrtTranslator::TranslateMeterEntry(const ::p4::v1::MeterEntry& entry,
                                             const bool& to_sdk) {
  ::absl::ReaderMutexLock l(&lock_);
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}
::util::StatusOr<::p4::v1::DirectMeterEntry>
P4RuntimeBfrtTranslator::TranslateDirectMeterEntry(
    const ::p4::v1::DirectMeterEntry& entry, const bool& to_sdk) {
  ::absl::ReaderMutexLock l(&lock_);
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}
::util::StatusOr<::p4::v1::CounterEntry>
P4RuntimeBfrtTranslator::TranslateCounterEntry(
    const ::p4::v1::CounterEntry& entry, const bool& to_sdk) {
  ::absl::ReaderMutexLock l(&lock_);
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}
::util::StatusOr<::p4::v1::DirectCounterEntry>
P4RuntimeBfrtTranslator::TranslateDirectCounterEntry(
    const ::p4::v1::DirectCounterEntry& entry, const bool& to_sdk) {
  ::absl::ReaderMutexLock l(&lock_);
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}
::util::StatusOr<::p4::v1::RegisterEntry>
P4RuntimeBfrtTranslator::TranslateRegisterEntry(
    const ::p4::v1::RegisterEntry& entry, const bool& to_sdk) {
  ::absl::ReaderMutexLock l(&lock_);
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}
::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>
P4RuntimeBfrtTranslator::TranslatePacketReplicationEngineEntry(
    const ::p4::v1::PacketReplicationEngineEntry& entry, const bool& to_sdk) {
  ::absl::ReaderMutexLock l(&lock_);
  // TODO(Yi Tseng): Will support this in another PR.
  return entry;
}

::util::StatusOr<std::string> P4RuntimeBfrtTranslator::TranslateValue(
    const std::string& value, const std::string& uri, const bool& to_sdk,
    const int32& bit_width) {
  if (uri.compare(kUriTnaPortId) == 0) {
    return TranslateTnaPortId(value, to_sdk, bit_width);
  }
  RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unknown URI: " << uri;
}

::util::StatusOr<std::string> P4RuntimeBfrtTranslator::TranslateTnaPortId(
    const std::string& value, const bool& to_sdk, const int32& bit_width) {
  // Translate type "tna/PortId_t"
  if (to_sdk) {
    (void)bit_width;  // Ignore this since we always translate the value to
                      // kTnaPortIdBitWidth.
    // singleton port id(N-byte) -> singleton port id(uint32) -> sdk port
    // id(uint32) -> sdk port id(2-byte)
    ASSIGN_OR_RETURN(const uint32 port_id, BytesToUint32(value));
    CHECK_RETURN_IF_FALSE(singleton_port_to_sdk_port_.count(port_id));
    const uint32 sdk_port_id = singleton_port_to_sdk_port_[port_id];
    ASSIGN_OR_RETURN(std::string sdk_port_id_bytes,
                     Uint32ToBytes(sdk_port_id, kTnaPortIdBitWidth));
    return sdk_port_id_bytes;
  } else {
    // sdk port id(2-byte) -> sdk port id(uint32) -> singleton port id(uint32)
    // -> singleton port id(N-byte)
    CHECK_RETURN_IF_FALSE(value.size() == 2);
    ASSIGN_OR_RETURN(const uint32 sdk_port_id, BytesToUint32(value));
    CHECK_RETURN_IF_FALSE(sdk_port_to_singleton_port_.count(sdk_port_id));
    const uint32 port_id = sdk_port_to_singleton_port_[sdk_port_id];
    ASSIGN_OR_RETURN(std::string port_id_bytes,
                     Uint32ToBytes(port_id, bit_width));
    return port_id_bytes;
  }
}

bool P4RuntimeBfrtTranslator::TranslationEnabled() {
  ::absl::ReaderMutexLock l(&lock_);
  return translation_enabled_;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
