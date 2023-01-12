// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_node.h"

#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bf_pipeline_utils.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/common/proto_oneof_writer_wrapper.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace hal {
namespace barefoot {

BfrtNode::BfrtNode(BfrtTableManager* bfrt_table_manager,
                   BfrtPacketioManager* bfrt_packetio_manager,
                   BfrtPreManager* bfrt_pre_manager,
                   BfrtCounterManager* bfrt_counter_manager,
                   BfrtP4RuntimeTranslator* bfrt_p4runtime_translator,
                   BfSdeInterface* bf_sde_interface, int device_id)
    : pipeline_initialized_(false),
      initialized_(false),
      bfrt_config_(),
      bf_sde_interface_(ABSL_DIE_IF_NULL(bf_sde_interface)),
      bfrt_table_manager_(ABSL_DIE_IF_NULL(bfrt_table_manager)),
      bfrt_packetio_manager_(bfrt_packetio_manager),
      bfrt_pre_manager_(ABSL_DIE_IF_NULL(bfrt_pre_manager)),
      bfrt_counter_manager_(ABSL_DIE_IF_NULL(bfrt_counter_manager)),
      bfrt_p4runtime_translator_(ABSL_DIE_IF_NULL(bfrt_p4runtime_translator)),
      node_id_(0),
      device_id_(device_id) {}

BfrtNode::BfrtNode()
    : pipeline_initialized_(false),
      initialized_(false),
      bfrt_config_(),
      bf_sde_interface_(nullptr),
      bfrt_table_manager_(nullptr),
      bfrt_packetio_manager_(nullptr),
      bfrt_pre_manager_(nullptr),
      bfrt_counter_manager_(nullptr),
      bfrt_p4runtime_translator_(nullptr),
      node_id_(0),
      device_id_(-1) {}

BfrtNode::~BfrtNode() = default;

// Factory function for creating the instance of the class.
std::unique_ptr<BfrtNode> BfrtNode::CreateInstance(
    BfrtTableManager* bfrt_table_manager,
    BfrtPacketioManager* bfrt_packetio_manager,
    BfrtPreManager* bfrt_pre_manager, BfrtCounterManager* bfrt_counter_manager,
    BfrtP4RuntimeTranslator* bfrt_p4runtime_translator,
    BfSdeInterface* bf_sde_interface, int device_id) {
  return absl::WrapUnique(
      new BfrtNode(bfrt_table_manager, bfrt_packetio_manager, bfrt_pre_manager,
                   bfrt_counter_manager, bfrt_p4runtime_translator,
                   bf_sde_interface, device_id));
}

::util::Status BfrtNode::PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id) {
  absl::WriterMutexLock l(&lock_);
  node_id_ = node_id;
  // RETURN_IF_ERROR(bfrt_table_manager_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(bfrt_packetio_manager_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(
      bfrt_p4runtime_translator_->PushChassisConfig(config, node_id));
  initialized_ = true;

  return ::util::OkStatus();
}

::util::Status BfrtNode::VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id) {
  // RETURN_IF_ERROR(bfrt_table_manager_->VerifyChassisConfig(config, node_id));
  RETURN_IF_ERROR(bfrt_packetio_manager_->VerifyChassisConfig(config, node_id));
  return ::util::OkStatus();
}

::util::Status BfrtNode::PushForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  // SaveForwardingPipelineConfig + CommitForwardingPipelineConfig
  RETURN_IF_ERROR(SaveForwardingPipelineConfig(config));
  return CommitForwardingPipelineConfig();
}

::util::Status BfrtNode::SaveForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  RETURN_IF_ERROR(VerifyForwardingPipelineConfig(config));
  BfPipelineConfig bf_config;
  RETURN_IF_ERROR(ExtractBfPipelineConfig(config, &bf_config));
  VLOG(2) << bf_config.DebugString();

  // Create internal BfrtDeviceConfig.
  BfrtDeviceConfig bfrt_config;
  auto program = bfrt_config.add_programs();
  program->set_name(bf_config.p4_name());
  program->set_bfrt(bf_config.bfruntime_info());
  *program->mutable_p4info() = config.p4info();
  for (const auto& profile : bf_config.profiles()) {
    auto pipeline = program->add_pipelines();
    pipeline->set_name(profile.profile_name());
    pipeline->set_context(profile.context());
    pipeline->set_config(profile.binary());
    *pipeline->mutable_scope() = profile.pipe_scope();
  }
  bfrt_config_ = bfrt_config;
  VLOG(2) << bfrt_config_.DebugString();

  return ::util::OkStatus();
}

::util::Status BfrtNode::CommitForwardingPipelineConfig() {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  RET_CHECK(bfrt_config_.programs_size() > 0);

  // Calling AddDevice() overwrites any previous pipeline.
  RETURN_IF_ERROR(bf_sde_interface_->AddDevice(device_id_, bfrt_config_));

  // Push pipeline config to the managers.
  const auto& p4info = bfrt_config_.programs(0).p4info();
  RETURN_IF_ERROR(
      bfrt_p4runtime_translator_->PushForwardingPipelineConfig(p4info));
  RETURN_IF_ERROR(
      bfrt_packetio_manager_->PushForwardingPipelineConfig(bfrt_config_));
  RETURN_IF_ERROR(
      bfrt_table_manager_->PushForwardingPipelineConfig(bfrt_config_));
  RETURN_IF_ERROR(
      bfrt_pre_manager_->PushForwardingPipelineConfig(bfrt_config_));
  RETURN_IF_ERROR(
      bfrt_counter_manager_->PushForwardingPipelineConfig(bfrt_config_));
  pipeline_initialized_ = true;
  return ::util::OkStatus();
}

::util::Status BfrtNode::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) const {
  RET_CHECK(config.has_p4info()) << "Missing P4 info";
  RET_CHECK(!config.p4_device_config().empty()) << "Missing P4 device config";
  BfPipelineConfig bf_config;
  RETURN_IF_ERROR(ExtractBfPipelineConfig(config, &bf_config));
  RETURN_IF_ERROR(bfrt_table_manager_->VerifyForwardingPipelineConfig(config));
  return ::util::OkStatus();
}

::util::Status BfrtNode::Shutdown() {
  absl::WriterMutexLock l(&lock_);
  auto status = ::util::OkStatus();
  // TODO(max): Check if we need to de-init the ASIC or SDE
  // TODO(max): Enable other Shutdown calls once implemented.
  APPEND_STATUS_IF_ERROR(status, bfrt_table_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, bfrt_packetio_manager_->Shutdown());
  // APPEND_STATUS_IF_ERROR(status, bfrt_pre_manager_->Shutdown());
  // APPEND_STATUS_IF_ERROR(status, bfrt_counter_manager_->Shutdown());

  pipeline_initialized_ = false;
  initialized_ = false;  // Set to false even if there is an error

  return status;
}

::util::Status BfrtNode::Freeze() { return ::util::OkStatus(); }

::util::Status BfrtNode::Unfreeze() { return ::util::OkStatus(); }

::util::Status BfrtNode::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  absl::WriterMutexLock l(&lock_);
  RET_CHECK(req.device_id() == node_id_)
      << "Request device id must be same as id of this BfrtNode.";
  RET_CHECK(req.atomicity() == ::p4::v1::WriteRequest::CONTINUE_ON_ERROR)
      << "Request atomicity "
      << ::p4::v1::WriteRequest::Atomicity_Name(req.atomicity())
      << " is not supported.";
  if (!initialized_ || !pipeline_initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  bool success = true;
  ASSIGN_OR_RETURN(auto session, bf_sde_interface_->CreateSession());
  RETURN_IF_ERROR(session->BeginBatch());
  for (const auto& update : req.updates()) {
    ::util::Status status = ::util::OkStatus();
    switch (update.entity().entity_case()) {
      case ::p4::v1::Entity::kTableEntry:
        status = bfrt_table_manager_->WriteTableEntry(
            session, update.type(), update.entity().table_entry());
        break;
      case ::p4::v1::Entity::kExternEntry:
        status = WriteExternEntry(session, update.type(),
                                  update.entity().extern_entry());
        break;
      case ::p4::v1::Entity::kActionProfileMember:
        status = bfrt_table_manager_->WriteActionProfileMember(
            session, update.type(), update.entity().action_profile_member());
        break;
      case ::p4::v1::Entity::kActionProfileGroup:
        status = bfrt_table_manager_->WriteActionProfileGroup(
            session, update.type(), update.entity().action_profile_group());
        break;
      case ::p4::v1::Entity::kPacketReplicationEngineEntry:
        status = bfrt_pre_manager_->WritePreEntry(
            session, update.type(),
            update.entity().packet_replication_engine_entry());
        break;
      case ::p4::v1::Entity::kDirectCounterEntry:
        status = bfrt_table_manager_->WriteDirectCounterEntry(
            session, update.type(), update.entity().direct_counter_entry());
        break;
      case ::p4::v1::Entity::kCounterEntry:
        status = bfrt_counter_manager_->WriteIndirectCounterEntry(
            session, update.type(), update.entity().counter_entry());
        break;
      case ::p4::v1::Entity::kRegisterEntry: {
        status = bfrt_table_manager_->WriteRegisterEntry(
            session, update.type(), update.entity().register_entry());
        break;
      }
      case ::p4::v1::Entity::kMeterEntry: {
        status = bfrt_table_manager_->WriteMeterEntry(
            session, update.type(), update.entity().meter_entry());
        break;
      }
      case ::p4::v1::Entity::kDigestEntry:
        status = bfrt_table_manager_->WriteDigestEntry(
            session, update.type(), update.entity().digest_entry());
        break;
      case ::p4::v1::Entity::kDirectMeterEntry:
      case ::p4::v1::Entity::kValueSetEntry:
      default:
        status = MAKE_ERROR(ERR_UNIMPLEMENTED)
                 << "Unsupported entity type: " << update.ShortDebugString();
        break;
    }
    success &= status.ok();
    results->push_back(status);
  }
  RETURN_IF_ERROR(session->EndBatch());

  if (!success) {
    return MAKE_ERROR(ERR_AT_LEAST_ONE_OPER_FAILED)
           << "One or more write operations failed.";
  }

  LOG(INFO) << "P4-based forwarding entities written successfully to node with "
            << "ID " << node_id_ << ".";
  return ::util::OkStatus();
}

::util::Status BfrtNode::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  RET_CHECK(writer) << "Channel writer must be non-null.";
  RET_CHECK(details) << "Details pointer must be non-null.";

  absl::ReaderMutexLock l(&lock_);
  RET_CHECK(req.device_id() == node_id_)
      << "Request device id must be same as id of this BfrtNode.";
  if (!initialized_ || !pipeline_initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ::p4::v1::ReadResponse resp;
  bool success = true;
  ASSIGN_OR_RETURN(auto session, bf_sde_interface_->CreateSession());
  for (const auto& entity : req.entities()) {
    switch (entity.entity_case()) {
      case ::p4::v1::Entity::kTableEntry: {
        auto status = bfrt_table_manager_->ReadTableEntry(
            session, entity.table_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kExternEntry: {
        auto status = ReadExternEntry(session, entity.extern_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kActionProfileMember: {
        auto status = bfrt_table_manager_->ReadActionProfileMember(
            session, entity.action_profile_member(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kActionProfileGroup: {
        auto status = bfrt_table_manager_->ReadActionProfileGroup(
            session, entity.action_profile_group(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kPacketReplicationEngineEntry: {
        auto status = bfrt_pre_manager_->ReadPreEntry(
            session, entity.packet_replication_engine_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kDirectCounterEntry: {
        auto status = bfrt_table_manager_->ReadDirectCounterEntry(
            session, entity.direct_counter_entry());
        if (!status.ok()) {
          success = false;
          details->push_back(status.status());
          break;
        }
        resp.add_entities()->mutable_direct_counter_entry()->CopyFrom(
            status.ValueOrDie());
        break;
      }
      case ::p4::v1::Entity::kCounterEntry: {
        auto status = bfrt_counter_manager_->ReadIndirectCounterEntry(
            session, entity.counter_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kRegisterEntry: {
        auto status = bfrt_table_manager_->ReadRegisterEntry(
            session, entity.register_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kMeterEntry: {
        auto status = bfrt_table_manager_->ReadMeterEntry(
            session, entity.meter_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kDigestEntry: {
        auto status = bfrt_table_manager_->ReadDigestEntry(
            session, entity.digest_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kDirectMeterEntry:
      case ::p4::v1::Entity::kValueSetEntry:
      default: {
        success = false;
        details->push_back(MAKE_ERROR(ERR_UNIMPLEMENTED)
                           << "Unsupported entity type: "
                           << entity.ShortDebugString());
        break;
      }
    }
  }
  RET_CHECK(writer->Write(resp)) << "Write to stream channel failed.";
  if (!success) {
    return MAKE_ERROR(ERR_AT_LEAST_ONE_OPER_FAILED)
           << "One or more read operations failed.";
  }
  return ::util::OkStatus();
}

::util::Status BfrtNode::RegisterStreamMessageResponseWriter(
    const std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>>&
        writer) {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  auto packet_in_writer =
      std::make_shared<ProtoOneofWriterWrapper<::p4::v1::StreamMessageResponse,
                                               ::p4::v1::PacketIn>>(
          writer, &::p4::v1::StreamMessageResponse::mutable_packet);
  RETURN_IF_ERROR(
      bfrt_packetio_manager_->RegisterPacketReceiveWriter(packet_in_writer));

  auto digest_list_writer =
      std::make_shared<ProtoOneofWriterWrapper<::p4::v1::StreamMessageResponse,
                                               ::p4::v1::DigestList>>(
          writer, &::p4::v1::StreamMessageResponse::mutable_digest);
  RETURN_IF_ERROR(
      bfrt_table_manager_->RegisterDigestListWriter(digest_list_writer));

  return ::util::OkStatus();
}

::util::Status BfrtNode::UnregisterStreamMessageResponseWriter() {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return bfrt_packetio_manager_->UnregisterPacketReceiveWriter();
}

::util::Status BfrtNode::HandleStreamMessageRequest(
    const ::p4::v1::StreamMessageRequest& req) {
  absl::ReaderMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  switch (req.update_case()) {
    case ::p4::v1::StreamMessageRequest::kPacket: {
      return bfrt_packetio_manager_->TransmitPacket(req.packet());
    }
    case ::p4::v1::StreamMessageRequest::kDigestAck:
      // TODO(max): implement digest ack handling.
      return ::util::OkStatus();
    default:
      return MAKE_ERROR(ERR_UNIMPLEMENTED)
             << "Unsupported StreamMessageRequest " << req.ShortDebugString()
             << ".";
  }
}

::util::Status BfrtNode::WriteExternEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type, const ::p4::v1::ExternEntry& entry) {
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId: {
      ::p4::v1::ActionProfileMember act_prof_member;
      RET_CHECK(entry.entry().UnpackTo(&act_prof_member))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile member.";
      return bfrt_table_manager_->WriteActionProfileMember(session, type,
                                                           act_prof_member);
    }
    case kTnaExternActionSelectorId: {
      ::p4::v1::ActionProfileGroup act_prof_group;
      RET_CHECK(entry.entry().UnpackTo(&act_prof_group))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile group.";
      return bfrt_table_manager_->WriteActionProfileGroup(session, type,
                                                          act_prof_group);
    }
    default:
      return MAKE_ERROR(ERR_UNIMPLEMENTED)
             << "Unsupported extern entry: " << entry.ShortDebugString() << ".";
  }
}

::util::Status BfrtNode::ReadExternEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::ExternEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId: {
      ::p4::v1::ActionProfileMember act_prof_member;
      RET_CHECK(entry.entry().UnpackTo(&act_prof_member))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile member";
      return bfrt_table_manager_->ReadActionProfileMember(
          session, act_prof_member, writer);
    }
    case kTnaExternActionSelectorId: {
      ::p4::v1::ActionProfileGroup act_prof_group;
      RET_CHECK(entry.entry().UnpackTo(&act_prof_group))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile group";
      return bfrt_table_manager_->ReadActionProfileGroup(
          session, act_prof_group, writer);
    }
    default:
      return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
             << "Unsupported extern entry: " << entry.ShortDebugString() << ".";
  }
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
