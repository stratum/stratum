// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/tdi/tdi_node.h"

#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/proto_oneof_writer_wrapper.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/tdi/tdi_pipeline_utils.h"
#include "stratum/hal/lib/tdi/tdi_sde_interface.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace hal {
namespace tdi {

TdiNode::TdiNode(TdiTableManager* tdi_table_manager,
                 TdiActionProfileManager* tdi_action_profile_manager,
                 TdiPacketioManager* tdi_packetio_manager,
                 TdiPreManager* tdi_pre_manager,
                 TdiCounterManager* tdi_counter_manager,
                 TdiSdeInterface* tdi_sde_interface, int device_id,
                 bool initialized, uint64 node_id)
    : pipeline_initialized_(false),
      initialized_(initialized),
      tdi_config_(),
      tdi_sde_interface_(ABSL_DIE_IF_NULL(tdi_sde_interface)),
      tdi_table_manager_(ABSL_DIE_IF_NULL(tdi_table_manager)),
      tdi_action_profile_manager_(
          ABSL_DIE_IF_NULL(tdi_action_profile_manager)),
      tdi_packetio_manager_(tdi_packetio_manager),
      tdi_pre_manager_(ABSL_DIE_IF_NULL(tdi_pre_manager)),
      tdi_counter_manager_(ABSL_DIE_IF_NULL(tdi_counter_manager)),
      node_id_(node_id),
      device_id_(device_id) {}

TdiNode::TdiNode()
    : pipeline_initialized_(false),
      initialized_(false),
      tdi_config_(),
      tdi_sde_interface_(nullptr),
      tdi_table_manager_(nullptr),
      tdi_action_profile_manager_(nullptr),
      tdi_packetio_manager_(nullptr),
      tdi_pre_manager_(nullptr),
      tdi_counter_manager_(nullptr),
      node_id_(0),
      device_id_(-1) {}

TdiNode::~TdiNode() = default;

// Factory function for creating the instance of the class.
std::unique_ptr<TdiNode> TdiNode::CreateInstance(
    TdiTableManager* tdi_table_manager,
    TdiActionProfileManager* tdi_action_profile_manager,
    TdiPacketioManager* tdi_packetio_manager,
    TdiPreManager* tdi_pre_manager,
    TdiCounterManager* tdi_counter_manager,
    TdiSdeInterface* tdi_sde_interface, int device_id,
    bool initialized, uint64 node_id) {
  return absl::WrapUnique(new TdiNode(
      tdi_table_manager, tdi_action_profile_manager, tdi_packetio_manager,
      tdi_pre_manager, tdi_counter_manager, tdi_sde_interface, device_id,
      initialized, node_id));
}

::util::Status TdiNode::PushChassisConfig(const ChassisConfig& config,
                                          uint64 node_id) {
  absl::WriterMutexLock l(&lock_);
  node_id_ = node_id;
  // RETURN_IF_ERROR(tdi_table_manager_->PushChassisConfig(config, node_id));
  // RETURN_IF_ERROR(
  //     tdi_action_profile_manager_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(tdi_packetio_manager_->PushChassisConfig(config, node_id));
  initialized_ = true;

  return ::util::OkStatus();
}

::util::Status TdiNode::VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id) {
  // RETURN_IF_ERROR(tdi_table_manager_->VerifyChassisConfig(config, node_id));
  // RETURN_IF_ERROR(
  //     tdi_action_profile_manager_->VerifyChassisConfig(config, node_id));
  RETURN_IF_ERROR(tdi_packetio_manager_->VerifyChassisConfig(config, node_id));
  return ::util::OkStatus();
}

::util::Status TdiNode::PushForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  // SaveForwardingPipelineConfig + CommitForwardingPipelineConfig
  RETURN_IF_ERROR(SaveForwardingPipelineConfig(config));
  return CommitForwardingPipelineConfig();
}

::util::Status TdiNode::SaveForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  RETURN_IF_ERROR(VerifyForwardingPipelineConfig(config));
  BfPipelineConfig bf_config;
  RETURN_IF_ERROR(ExtractBfPipelineConfig(config, &bf_config));
  VLOG(2) << bf_config.DebugString();

  // Create internal TdiDeviceConfig.
  TdiDeviceConfig tdi_config;
  auto program = tdi_config.add_programs();
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
  tdi_config_ = tdi_config;
  VLOG(2) << tdi_config_.DebugString();

  return ::util::OkStatus();
}

::util::Status TdiNode::CommitForwardingPipelineConfig() {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  CHECK_RETURN_IF_FALSE(tdi_config_.programs_size() > 0);

  // Calling AddDevice() overwrites any previous pipeline.
  RETURN_IF_ERROR(tdi_sde_interface_->AddDevice(device_id_, tdi_config_));

  // Push pipeline config to the managers.
  RETURN_IF_ERROR(
      tdi_packetio_manager_->PushForwardingPipelineConfig(tdi_config_));
  RETURN_IF_ERROR(
      tdi_table_manager_->PushForwardingPipelineConfig(tdi_config_));
  RETURN_IF_ERROR(
      tdi_action_profile_manager_->PushForwardingPipelineConfig(tdi_config_));
  RETURN_IF_ERROR(
      tdi_pre_manager_->PushForwardingPipelineConfig(tdi_config_));
  RETURN_IF_ERROR(
      tdi_counter_manager_->PushForwardingPipelineConfig(tdi_config_));

  pipeline_initialized_ = true;
  return ::util::OkStatus();
}

::util::Status TdiNode::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) const {
  CHECK_RETURN_IF_FALSE(config.has_p4info()) << "Missing P4 info";
  CHECK_RETURN_IF_FALSE(!config.p4_device_config().empty())
      << "Missing P4 device config";
  BfPipelineConfig bf_config;
  RETURN_IF_ERROR(ExtractBfPipelineConfig(config, &bf_config));
  RETURN_IF_ERROR(tdi_table_manager_->VerifyForwardingPipelineConfig(config));
  return ::util::OkStatus();
}

::util::Status TdiNode::Shutdown() {
  absl::WriterMutexLock l(&lock_);
  auto status = ::util::OkStatus();
  // TODO(max): Check if we need to de-init the ASIC or SDE
  // TODO(max): Enable other Shutdown calls once implemented.
  // APPEND_STATUS_IF_ERROR(status, tdi_table_manager_->Shutdown());
  // APPEND_STATUS_IF_ERROR(status, tdi_action_profile_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, tdi_packetio_manager_->Shutdown());
  // APPEND_STATUS_IF_ERROR(status, tdi_pre_manager_->Shutdown());
  // APPEND_STATUS_IF_ERROR(status, tdi_counter_manager_->Shutdown());

  pipeline_initialized_ = false;
  initialized_ = false;  // Set to false even if there is an error

  return status;
}

::util::Status TdiNode::Freeze() { return ::util::OkStatus(); }

::util::Status TdiNode::Unfreeze() { return ::util::OkStatus(); }

::util::Status TdiNode::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(req.device_id() == node_id_)
      << "Request device id must be same as id of this TdiNode.";
  CHECK_RETURN_IF_FALSE(req.atomicity() ==
                        ::p4::v1::WriteRequest::CONTINUE_ON_ERROR)
      << "Request atomicity "
      << ::p4::v1::WriteRequest::Atomicity_Name(req.atomicity())
      << " is not supported.";
  if (!initialized_ || !pipeline_initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  bool success = true;
  ASSIGN_OR_RETURN(auto session, tdi_sde_interface_->CreateSession());
  RETURN_IF_ERROR(session->BeginBatch());
  for (const auto& update : req.updates()) {
    ::util::Status status = ::util::OkStatus();
    switch (update.entity().entity_case()) {
      case ::p4::v1::Entity::kTableEntry:
        status = tdi_table_manager_->WriteTableEntry(
            session, update.type(), update.entity().table_entry());
        break;
      case ::p4::v1::Entity::kExternEntry:
        status = WriteExternEntry(session, update.type(),
                                  update.entity().extern_entry());
        break;
      case ::p4::v1::Entity::kActionProfileMember:
        status = tdi_action_profile_manager_->WriteActionProfileMember(
            session, update.type(), update.entity().action_profile_member());
        break;
      case ::p4::v1::Entity::kActionProfileGroup:
        status = tdi_action_profile_manager_->WriteActionProfileGroup(
            session, update.type(), update.entity().action_profile_group());
        break;
      case ::p4::v1::Entity::kPacketReplicationEngineEntry:
        status = tdi_pre_manager_->WritePreEntry(
            session, update.type(),
            update.entity().packet_replication_engine_entry());
        break;
      case ::p4::v1::Entity::kDirectCounterEntry:
        status = tdi_table_manager_->WriteDirectCounterEntry(
            session, update.type(), update.entity().direct_counter_entry());
        break;
      case ::p4::v1::Entity::kCounterEntry:
        status = tdi_counter_manager_->WriteIndirectCounterEntry(
            session, update.type(), update.entity().counter_entry());
        break;
      case ::p4::v1::Entity::kRegisterEntry: {
        status = tdi_table_manager_->WriteRegisterEntry(
            session, update.type(), update.entity().register_entry());
        break;
      }
      case ::p4::v1::Entity::kMeterEntry: {
        status = tdi_table_manager_->WriteMeterEntry(
            session, update.type(), update.entity().meter_entry());
        break;
      }
      case ::p4::v1::Entity::kDirectMeterEntry:
      case ::p4::v1::Entity::kValueSetEntry:
      case ::p4::v1::Entity::kDigestEntry:
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

::util::Status TdiNode::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  CHECK_RETURN_IF_FALSE(writer) << "Channel writer must be non-null.";
  CHECK_RETURN_IF_FALSE(details) << "Details pointer must be non-null.";

  absl::ReaderMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(req.device_id() == node_id_)
      << "Request device id must be same as id of this TdiNode.";
  if (!initialized_ || !pipeline_initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ::p4::v1::ReadResponse resp;
  bool success = true;
  ASSIGN_OR_RETURN(auto session, tdi_sde_interface_->CreateSession());
  for (const auto& entity : req.entities()) {
    switch (entity.entity_case()) {
      case ::p4::v1::Entity::kTableEntry: {
        auto status = tdi_table_manager_->ReadTableEntry(
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
        auto status = tdi_action_profile_manager_->ReadActionProfileMember(
            session, entity.action_profile_member(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kActionProfileGroup: {
        auto status = tdi_action_profile_manager_->ReadActionProfileGroup(
            session, entity.action_profile_group(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kPacketReplicationEngineEntry: {
        auto status = tdi_pre_manager_->ReadPreEntry(
            session, entity.packet_replication_engine_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kDirectCounterEntry: {
        auto status = tdi_table_manager_->ReadDirectCounterEntry(
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
        auto status = tdi_counter_manager_->ReadIndirectCounterEntry(
            session, entity.counter_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kRegisterEntry: {
        auto status = tdi_table_manager_->ReadRegisterEntry(
            session, entity.register_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kMeterEntry: {
        auto status = tdi_table_manager_->ReadMeterEntry(
            session, entity.meter_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kDirectMeterEntry:
      case ::p4::v1::Entity::kValueSetEntry:
      case ::p4::v1::Entity::kDigestEntry:
      default: {
        success = false;
        details->push_back(MAKE_ERROR(ERR_UNIMPLEMENTED)
                           << "Unsupported entity type: "
                           << entity.ShortDebugString());
        break;
      }
    }
  }
  CHECK_RETURN_IF_FALSE(writer->Write(resp))
      << "Write to stream channel failed.";
  if (!success) {
    return MAKE_ERROR(ERR_AT_LEAST_ONE_OPER_FAILED)
           << "One or more read operations failed.";
  }
  return ::util::OkStatus();
}

::util::Status TdiNode::RegisterStreamMessageResponseWriter(
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

  return tdi_packetio_manager_->RegisterPacketReceiveWriter(packet_in_writer);
}

::util::Status TdiNode::UnregisterStreamMessageResponseWriter() {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return tdi_packetio_manager_->UnregisterPacketReceiveWriter();
}

::util::Status TdiNode::HandleStreamMessageRequest(
    const ::p4::v1::StreamMessageRequest& req) {
  absl::ReaderMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  switch (req.update_case()) {
    case ::p4::v1::StreamMessageRequest::kPacket: {
      return tdi_packetio_manager_->TransmitPacket(req.packet());
    }
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unsupported StreamMessageRequest "
                                      << req.ShortDebugString() << ".";
  }
}

::util::Status TdiNode::WriteExternEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type, const ::p4::v1::ExternEntry& entry) {
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId:
    case kTnaExternActionSelectorId:
      return tdi_action_profile_manager_->WriteActionProfileEntry(session,
                                                                   type, entry);
    default:
      RETURN_ERROR() << "Unsupported extern entry: " << entry.ShortDebugString()
                     << ".";
  }
}

::util::Status TdiNode::ReadExternEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::ExternEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId:
    case kTnaExternActionSelectorId:
      return tdi_action_profile_manager_->ReadActionProfileEntry(
          session, entry, writer);
    default:
      RETURN_ERROR(ERR_OPER_NOT_SUPPORTED)
          << "Unsupported extern entry: " << entry.ShortDebugString() << ".";
  }
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
