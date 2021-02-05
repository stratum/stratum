// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_node.h"

#include <set>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/macros.h"

// TODO(unknown): This flag is currently false to skip static entry writes
// until all related hardware tables and related mapping are implemented.
DEFINE_bool(enable_static_table_writes, true,
            "Enables writes of static table "
            "entries from the P4 pipeline config to the hardware tables");

namespace stratum {
namespace hal {
namespace bcm {

BcmNode::BcmNode(BcmAclManager* bcm_acl_manager, BcmL2Manager* bcm_l2_manager,
                 BcmL3Manager* bcm_l3_manager,
                 BcmPacketioManager* bcm_packetio_manager,
                 BcmTableManager* bcm_table_manager,
                 BcmTunnelManager* bcm_tunnel_manager,
                 P4TableMapper* p4_table_mapper, int unit)
    : initialized_(false),
      bcm_acl_manager_(ABSL_DIE_IF_NULL(bcm_acl_manager)),
      bcm_l2_manager_(ABSL_DIE_IF_NULL(bcm_l2_manager)),
      bcm_l3_manager_(ABSL_DIE_IF_NULL(bcm_l3_manager)),
      bcm_packetio_manager_(ABSL_DIE_IF_NULL(bcm_packetio_manager)),
      bcm_table_manager_(ABSL_DIE_IF_NULL(bcm_table_manager)),
      bcm_tunnel_manager_(ABSL_DIE_IF_NULL(bcm_tunnel_manager)),
      p4_table_mapper_(ABSL_DIE_IF_NULL(p4_table_mapper)),
      node_id_(0),
      unit_(unit) {}

BcmNode::BcmNode()
    : initialized_(false),
      bcm_acl_manager_(nullptr),
      bcm_l2_manager_(nullptr),
      bcm_l3_manager_(nullptr),
      bcm_packetio_manager_(nullptr),
      bcm_table_manager_(nullptr),
      bcm_tunnel_manager_(nullptr),
      p4_table_mapper_(nullptr),
      node_id_(0),
      unit_(-1) {}

BcmNode::~BcmNode() {}

::util::Status BcmNode::PushChassisConfig(const ChassisConfig& config,
                                          uint64 node_id) {
  absl::WriterMutexLock l(&lock_);
  node_id_ = node_id;
  RETURN_IF_ERROR(p4_table_mapper_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(bcm_table_manager_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(bcm_l2_manager_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(bcm_l3_manager_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(bcm_acl_manager_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(bcm_tunnel_manager_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(bcm_packetio_manager_->PushChassisConfig(config, node_id));
  initialized_ = true;

  return ::util::OkStatus();
}

::util::Status BcmNode::VerifyChassisConfig(const ChassisConfig& config,
                                            uint64 node_id) {
  absl::ReaderMutexLock l(&lock_);
  ::util::Status status = ::util::OkStatus();
  if (node_id == 0) {
    ::util::Status error = MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid node ID.";
    APPEND_STATUS_IF_ERROR(status, error);
  }
  if (initialized_ && node_id_ != node_id) {
    ::util::Status error = MAKE_ERROR(ERR_REBOOT_REQUIRED)
                           << "Detected a change in the node_id (" << node_id_
                           << " vs " << node_id
                           << ") after the class was initialized.";
    APPEND_STATUS_IF_ERROR(status, error);
  }
  APPEND_STATUS_IF_ERROR(
      status, p4_table_mapper_->VerifyChassisConfig(config, node_id));
  APPEND_STATUS_IF_ERROR(
      status, bcm_table_manager_->VerifyChassisConfig(config, node_id));
  APPEND_STATUS_IF_ERROR(status,
                         bcm_l2_manager_->VerifyChassisConfig(config, node_id));
  APPEND_STATUS_IF_ERROR(status,
                         bcm_l3_manager_->VerifyChassisConfig(config, node_id));
  APPEND_STATUS_IF_ERROR(
      status, bcm_acl_manager_->VerifyChassisConfig(config, node_id));
  APPEND_STATUS_IF_ERROR(
      status, bcm_tunnel_manager_->VerifyChassisConfig(config, node_id));
  APPEND_STATUS_IF_ERROR(
      status, bcm_packetio_manager_->VerifyChassisConfig(config, node_id));

  return status;
}

::util::Status BcmNode::PushForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&lock_);
  P4PipelineConfig p4_pipeline_config;
  CHECK_RETURN_IF_FALSE(
      p4_pipeline_config.ParseFromString(config.p4_device_config()))
      << "Failed to parse p4_device_config byte stream for node with ID "
      << node_id_ << ".";
  RETURN_IF_ERROR(StaticEntryWrite(p4_pipeline_config, /*post_push=*/false));
  RETURN_IF_ERROR(p4_table_mapper_->PushForwardingPipelineConfig(config));
  RETURN_IF_ERROR(bcm_acl_manager_->PushForwardingPipelineConfig(config));
  RETURN_IF_ERROR(bcm_tunnel_manager_->PushForwardingPipelineConfig(config));
  RETURN_IF_ERROR(StaticEntryWrite(p4_pipeline_config, /*post_push=*/true));

  return ::util::OkStatus();
}

::util::Status BcmNode::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::ReaderMutexLock l(&lock_);
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(
      status, p4_table_mapper_->VerifyForwardingPipelineConfig(config));
  APPEND_STATUS_IF_ERROR(
      status, bcm_acl_manager_->VerifyForwardingPipelineConfig(config));
  APPEND_STATUS_IF_ERROR(
      status, bcm_tunnel_manager_->VerifyForwardingPipelineConfig(config));

  return status;
}

::util::Status BcmNode::Shutdown() {
  absl::WriterMutexLock l(&lock_);
  auto status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, bcm_packetio_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, bcm_tunnel_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, bcm_acl_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, bcm_l3_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, bcm_l2_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, bcm_table_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, p4_table_mapper_->Shutdown());
  initialized_ = false;  // Set to false even if there is an error

  return status;
}

::util::Status BcmNode::Freeze() {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::util::Status BcmNode::Unfreeze() {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::util::Status BcmNode::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  CHECK_RETURN_IF_FALSE(results) << "Results pointer must be non-null.";

  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(req.device_id() == node_id_)
      << "Request device id must be same as id of this BcmNode.";
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  return DoWriteForwardingEntries(req, results);
}

::util::Status BcmNode::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  CHECK_RETURN_IF_FALSE(writer) << "Channel writer must be non-null.";
  CHECK_RETURN_IF_FALSE(details) << "Details pointer must be non-null.";

  absl::ReaderMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(req.device_id() == node_id_)
      << "Request device id must be same as id of this BcmNode.";
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  std::set<uint32> table_ids = {};
  std::set<uint32> action_profile_ids = {};
  std::set<uint32> clone_session_ids = {};
  std::set<uint32> multicast_group_ids = {};
  bool table_entries_requested = false;
  bool action_profile_members_requested = false;
  bool action_profile_groups_requested = false;
  bool clone_sessions_requested = false;
  bool multicast_groups_requested = false;
  for (const auto& entity : req.entities()) {
    ::util::Status status = ::util::OkStatus();
    switch (entity.entity_case()) {
      case ::p4::v1::Entity::kExternEntry:
        // TODO(unknown): Implement this.
        return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
               << "Extern entries are not currently supported.";
      case ::p4::v1::Entity::kTableEntry:
        table_ids.insert(entity.table_entry().table_id());
        table_entries_requested = true;
        break;
      case ::p4::v1::Entity::kActionProfileMember:
        action_profile_ids.insert(
            entity.action_profile_member().action_profile_id());
        action_profile_members_requested = true;
        break;
      case ::p4::v1::Entity::kActionProfileGroup:
        action_profile_ids.insert(
            entity.action_profile_group().action_profile_id());
        action_profile_groups_requested = true;
        break;
      case ::p4::v1::Entity::kPacketReplicationEngineEntry:
        if (entity.packet_replication_engine_entry()
                .has_multicast_group_entry()) {
          multicast_group_ids.insert(entity.packet_replication_engine_entry()
                                         .multicast_group_entry()
                                         .multicast_group_id());
          multicast_groups_requested = true;
        }
        if (entity.packet_replication_engine_entry()
                .has_clone_session_entry()) {
          clone_session_ids.insert(entity.packet_replication_engine_entry()
                                       .clone_session_entry()
                                       .session_id());
          clone_sessions_requested = true;
        }
        break;
      case ::p4::v1::Entity::kMeterEntry:
        // TODO(unknown): Implement this.
        status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                 << "Meter entries are not currently supported: "
                 << entity.ShortDebugString() << ".";
        if (details != nullptr) details->push_back(status);
        break;
      case ::p4::v1::Entity::kDirectMeterEntry:
        // TODO(unknown): Implement this.
        status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                 << "Direct meter entries are not currently supported: "
                 << entity.ShortDebugString() << ".";
        if (details != nullptr) details->push_back(status);
        break;
      case ::p4::v1::Entity::kCounterEntry:
        // TODO(unknown): Implement this.
        status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                 << "Counter entries are not currently supported: "
                 << entity.ShortDebugString() << ".";
        if (details != nullptr) details->push_back(status);
        break;
      case ::p4::v1::Entity::kDirectCounterEntry: {
        // Attempt to read ACL stats for table entry identified in request.
        ::p4::v1::ReadResponse resp;
        ::p4::v1::CounterData* counter =
            resp.add_entities()->mutable_direct_counter_entry()->mutable_data();
        RETURN_IF_ERROR(bcm_acl_manager_->GetTableEntryStats(
            entity.direct_counter_entry().table_entry(), counter));
        if (!writer->Write(resp)) {
          return MAKE_ERROR(ERR_INTERNAL)
                 << "Write to stream for failed for node " << node_id_ << ".";
        }
        break;
      }
      case ::p4::v1::Entity::ENTITY_NOT_SET:
        status = MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Empty entity: " << entity.ShortDebugString() << ".";
        if (details != nullptr) details->push_back(status);
        break;
      default:
        status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                 << "Unsupported entity type " << entity.entity_case()
                 << " with no plan of support: " << entity.ShortDebugString()
                 << ".";
        if (details != nullptr) details->push_back(status);
    }
  }

  if (table_ids.count(0)) table_ids.clear();                    // request all
  if (action_profile_ids.count(0)) action_profile_ids.clear();  // request all

  if (table_entries_requested) {
    ::p4::v1::ReadResponse resp;
    std::vector<::p4::v1::TableEntry*> acl_flows;
    // Populate response with table entries and obtain list of pointers into the
    // response to entries for which stats need to be collected.
    RETURN_IF_ERROR(
        bcm_table_manager_->ReadTableEntries(table_ids, &resp, &acl_flows));
    // Collect ACL stats.
    for (auto* flow : acl_flows) {
      RETURN_IF_ERROR(bcm_acl_manager_->GetTableEntryStats(
          *flow, flow->mutable_counter_data()));
    }
    if (!writer->Write(resp)) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Write to stream for failed for node " << node_id_ << ".";
    }
  }
  if (action_profile_members_requested) {
    RETURN_IF_ERROR(bcm_table_manager_->ReadActionProfileMembers(
        action_profile_ids, writer));
  }
  if (action_profile_groups_requested) {
    RETURN_IF_ERROR(bcm_table_manager_->ReadActionProfileGroups(
        action_profile_ids, writer));
  }
  if (clone_sessions_requested) {
    RETURN_IF_ERROR(
        bcm_table_manager_->ReadMulticastGroups(clone_session_ids, writer));
  }
  if (multicast_groups_requested) {
    RETURN_IF_ERROR(
        bcm_table_manager_->ReadCloneSessions(multicast_group_ids, writer));
  }

  return ::util::OkStatus();
}

::util::Status BcmNode::RegisterStreamMessageResponseWriter(
    const std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>>&
        writer) {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  auto packet_in_writer =
      std::make_shared<ConstraintWriterWrapper<::p4::v1::StreamMessageResponse,
                                               ::p4::v1::PacketIn>>(
          writer, &::p4::v1::StreamMessageResponse::mutable_packet);

  return bcm_packetio_manager_->RegisterPacketReceiveWriter(
      GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, packet_in_writer);
}

::util::Status BcmNode::UnregisterStreamMessageResponseWriter() {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  return bcm_packetio_manager_->UnregisterPacketReceiveWriter(
      GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER);
}

::util::Status BcmNode::SendStreamMessageRequest(
    const ::p4::v1::StreamMessageRequest& req) {
  absl::ReaderMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  switch (req.update_case()) {
    case ::p4::v1::StreamMessageRequest::kPacket: {
      return bcm_packetio_manager_->TransmitPacket(
          GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, req.packet());
    }
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unsupported StreamMessageRequest "
                                      << req.ShortDebugString() << ".";
  }
}

::util::Status BcmNode::UpdatePortState(uint32 port_id) {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  // Reprogram all multipath groups referencing this port.
  RETURN_IF_ERROR(bcm_l3_manager_->UpdateMultipathGroupsForPort(port_id));
  return ::util::OkStatus();
}

std::unique_ptr<BcmNode> BcmNode::CreateInstance(
    BcmAclManager* bcm_acl_manager, BcmL2Manager* bcm_l2_manager,
    BcmL3Manager* bcm_l3_manager, BcmPacketioManager* bcm_packetio_manager,
    BcmTableManager* bcm_table_manager, BcmTunnelManager* bcm_tunnel_manager,
    P4TableMapper* p4_table_mapper, int unit) {
  return absl::WrapUnique(new BcmNode(
      bcm_acl_manager, bcm_l2_manager, bcm_l3_manager, bcm_packetio_manager,
      bcm_table_manager, bcm_tunnel_manager, p4_table_mapper, unit));
}

::util::Status BcmNode::StaticEntryWrite(const P4PipelineConfig& config,
                                         bool post_push) {
  ::p4::v1::WriteRequest static_write_request;

  // Separate sets of static entries apply before and after the
  // ForwardingPipelineConfig change takes place.
  if (post_push) {
    RETURN_IF_ERROR(p4_table_mapper_->HandlePostPushStaticEntryChanges(
        config.static_table_entries(), &static_write_request));
  } else {
    RETURN_IF_ERROR(p4_table_mapper_->HandlePrePushStaticEntryChanges(
        config.static_table_entries(), &static_write_request));
  }

  if (static_write_request.updates_size() == 0) {
    return ::util::OkStatus();
  }

  if (!FLAGS_enable_static_table_writes) {
    LOG(WARNING) << "Skipping writes for "
                 << static_write_request.updates_size()
                 << " static table entries";
    return ::util::OkStatus();
  }

  // The static entries get written to hardware tables as if they came
  // via a normal P4 WriteRequest RPC, except that p4_table_mapper_ needs
  // to be told that it's OK to change the static tables for this one request.
  p4_table_mapper_->EnableStaticTableUpdates();
  std::vector<::util::Status> static_results;
  ::util::Status static_status =
      DoWriteForwardingEntries(static_write_request, &static_results);
  // TODO(unknown): The per-entry results ultimately should be folded into
  // static_status for return with the overall pipeline config push result.
  if (!static_status.ok()) {
    for (const auto& entry_result : static_results) {
      if (!entry_result.ok()) {
        LOG(ERROR) << "Static table entry error is "
                   << entry_result.error_message();
      }
    }
  }
  p4_table_mapper_->DisableStaticTableUpdates();

  return static_status;
}

::util::Status BcmNode::DoWriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  bool success = true;
  for (const auto& update : req.updates()) {
    ::util::Status status = ::util::OkStatus();
    switch (update.entity().entity_case()) {
      case ::p4::v1::Entity::kExternEntry:
        // TODO(unknown): Implement this.
        status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                 << "Extern entries are not currently supported.";
        break;
      case ::p4::v1::Entity::kTableEntry:
        status = TableWrite(update.entity().table_entry(), update.type());
        break;
      case ::p4::v1::Entity::kActionProfileMember:
        status = ActionProfileMemberWrite(
            update.entity().action_profile_member(), update.type());
        break;
      case ::p4::v1::Entity::kActionProfileGroup:
        status = ActionProfileGroupWrite(update.entity().action_profile_group(),
                                         update.type());
        break;
      case ::p4::v1::Entity::kMeterEntry:
        // TODO(unknown): Implement this.
        status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                 << "Meter entries are not currently supported: "
                 << update.ShortDebugString() << ".";
        break;
      case ::p4::v1::Entity::kDirectMeterEntry:
        // For direct meter entry, only modify action is expected.
        if (update.type() != ::p4::v1::Update::MODIFY) {
          status = MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Direct meter entries can only be modified: "
                   << update.ShortDebugString() << ".";
        } else {
          status = bcm_acl_manager_->UpdateTableEntryMeter(
              update.entity().direct_meter_entry());
        }
        break;
      case ::p4::v1::Entity::kCounterEntry:
        // TODO(unknown): Implement this.
        status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                 << "Counter entries are not currently supported: "
                 << update.ShortDebugString() << ".";
        break;
      case ::p4::v1::Entity::kDirectCounterEntry:
        // TODO(unknown): Implement this.
        status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                 << "Direct counter entries are not currently supported: "
                 << update.ShortDebugString() << ".";
        break;
      case ::p4::v1::Entity::kPacketReplicationEngineEntry:
        status = PacketReplicationEngineEntryWrite(
            update.entity().packet_replication_engine_entry(), update.type());
        break;
      case ::p4::v1::Entity::ENTITY_NOT_SET:
        status = MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Empty entity: " << update.ShortDebugString() << ".";
        break;
      default:
        status = MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                 << "Unsupported entity type " << update.entity().entity_case()
                 << " with no plan of support: " << update.ShortDebugString()
                 << ".";
    }
    success &= status.ok();
    results->push_back(status);
  }

  if (!success) {
    return MAKE_ERROR(ERR_AT_LEAST_ONE_OPER_FAILED)
           << "One or more write operations failed.";
  }

  LOG(INFO) << "P4-based forwarding entities written successfully to node with "
            << "ID " << node_id_ << ".";

  return ::util::OkStatus();
}

// TODO(unknown): Complete this function for all the update types.
::util::Status BcmNode::TableWrite(const ::p4::v1::TableEntry& entry,
                                   ::p4::v1::Update::Type type) {
  CHECK_RETURN_IF_FALSE(type != ::p4::v1::Update::UNSPECIFIED);

  // We populate BcmFlowEntry based on the given TableEntry.
  BcmFlowEntry bcm_flow_entry;
  RETURN_IF_ERROR(
      bcm_table_manager_->FillBcmFlowEntry(entry, type, &bcm_flow_entry));
  BcmFlowEntry::BcmTableType bcm_table_type = bcm_flow_entry.bcm_table_type();
  // Try to program the flow.
  bool consumed = false;  // will be set to true if we know what to do
  switch (type) {
    case ::p4::v1::Update::INSERT: {
      switch (bcm_table_type) {
        case BcmFlowEntry::BCM_TABLE_IPV4_LPM:
        case BcmFlowEntry::BCM_TABLE_IPV4_HOST:
        case BcmFlowEntry::BCM_TABLE_IPV6_LPM:
        case BcmFlowEntry::BCM_TABLE_IPV6_HOST:
          RETURN_IF_ERROR(bcm_l3_manager_->InsertTableEntry(entry));
          // BcmL3Manager updates the internal records in BcmTableManager.
          consumed = true;
          break;
        // TODO(richardyu): Move BcmTableManager calls into BcmL2Manager.
        case BcmFlowEntry::BCM_TABLE_L2_MULTICAST:
          RETURN_IF_ERROR(
              bcm_l2_manager_->InsertMulticastGroup(bcm_flow_entry));
          // Update the internal records in BcmTableManager.
          RETURN_IF_ERROR(bcm_table_manager_->AddTableEntry(entry));
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_L2_UNICAST:
          RETURN_IF_ERROR(bcm_l2_manager_->InsertL2Entry(bcm_flow_entry));
          // Update the internal records in BcmTableManager.
          RETURN_IF_ERROR(bcm_table_manager_->AddTableEntry(entry));
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_MY_STATION:
          RETURN_IF_ERROR(
              bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry));
          // Update the internal records in BcmTableManager.
          RETURN_IF_ERROR(bcm_table_manager_->AddTableEntry(entry));
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_ACL:
          RETURN_IF_ERROR(bcm_acl_manager_->InsertTableEntry(entry));
          // BcmAclManager updates BcmTableManager.
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_TUNNEL:
          RETURN_IF_ERROR(bcm_tunnel_manager_->InsertTableEntry(entry));
          consumed = true;
          break;
        default:
          break;
      }
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      switch (bcm_table_type) {
        case BcmFlowEntry::BCM_TABLE_IPV4_LPM:
        case BcmFlowEntry::BCM_TABLE_IPV4_HOST:
        case BcmFlowEntry::BCM_TABLE_IPV6_LPM:
        case BcmFlowEntry::BCM_TABLE_IPV6_HOST:
          RETURN_IF_ERROR(bcm_l3_manager_->ModifyTableEntry(entry));
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_ACL:
          RETURN_IF_ERROR(bcm_acl_manager_->ModifyTableEntry(entry));
          // BcmAclManager updates BcmTableManager.
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_TUNNEL:
          RETURN_IF_ERROR(bcm_tunnel_manager_->ModifyTableEntry(entry));
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_L2_UNICAST:
          // TODO(max): do we need modify?
          // RETURN_IF_ERROR(
          //     bcm_l2_manager_->ModifyL2Entry(bcm_flow_entry));
          // Update the internal records in BcmTableManager.
          // RETURN_IF_ERROR(bcm_table_manager_->ModifyTableEntry(entry));
          // consumed = true;
          break;
        default:
          break;
      }
      break;
    }
    case ::p4::v1::Update::DELETE: {
      switch (bcm_table_type) {
        case BcmFlowEntry::BCM_TABLE_IPV4_LPM:
        case BcmFlowEntry::BCM_TABLE_IPV4_HOST:
        case BcmFlowEntry::BCM_TABLE_IPV6_LPM:
        case BcmFlowEntry::BCM_TABLE_IPV6_HOST:
          RETURN_IF_ERROR(bcm_l3_manager_->DeleteTableEntry(entry));
          // BcmL3Manager updates the internal records in BcmTableManager.
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_L2_MULTICAST:
          RETURN_IF_ERROR(
              bcm_l2_manager_->DeleteMulticastGroup(bcm_flow_entry));
          // Update the internal records in BcmTableManager.
          RETURN_IF_ERROR(bcm_table_manager_->DeleteTableEntry(entry));
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_L2_UNICAST:
          RETURN_IF_ERROR(bcm_l2_manager_->DeleteL2Entry(bcm_flow_entry));
          // Update the internal records in BcmTableManager.
          RETURN_IF_ERROR(bcm_table_manager_->DeleteTableEntry(entry));
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_MY_STATION:
          RETURN_IF_ERROR(
              bcm_l2_manager_->DeleteMyStationEntry(bcm_flow_entry));
          // Update the internal records in BcmTableManager.
          RETURN_IF_ERROR(bcm_table_manager_->DeleteTableEntry(entry));
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_ACL:
          RETURN_IF_ERROR(bcm_acl_manager_->DeleteTableEntry(entry));
          // BcmAclManager updates BcmTableManager.
          consumed = true;
          break;
        case BcmFlowEntry::BCM_TABLE_TUNNEL:
          RETURN_IF_ERROR(bcm_tunnel_manager_->DeleteTableEntry(entry));
          consumed = true;
          break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }

  CHECK_RETURN_IF_FALSE(consumed)
      << "Do not know what to do with the following BcmTableType when doing "
      << "table update of type " << ::p4::v1::Update::Type_Name(type) << ": "
      << BcmFlowEntry::BcmTableType_Name(bcm_table_type)
      << ". ::p4::v1::TableEntry: " << entry.ShortDebugString() << ".";

  return ::util::OkStatus();
}

::util::Status BcmNode::ActionProfileMemberWrite(
    const ::p4::v1::ActionProfileMember& member, ::p4::v1::Update::Type type) {
  bool consumed = false;  // will be set to true if we know what to do
  // Here, we only support ActionProfiles for nexthop members which will be part
  // of an ECMP/WCMP group.
  uint32 member_id = member.member_id();
  switch (type) {
    case ::p4::v1::Update::INSERT: {
      // Member must not exist. Instead of re-add, controller must use modify.
      CHECK_RETURN_IF_FALSE(
          !bcm_table_manager_->ActionProfileMemberExists(member_id))
          << "member_id " << member_id << " already exists on node " << node_id_
          << ". ActionProfileMember: " << member.ShortDebugString() << ".";
      // Fill BcmNonMultipathNexthop for this member and add it to the HW.
      BcmNonMultipathNexthop nexthop;
      RETURN_IF_ERROR(
          bcm_table_manager_->FillBcmNonMultipathNexthop(member, &nexthop));
      ASSIGN_OR_RETURN(
          int egress_intf_id,
          bcm_l3_manager_->FindOrCreateNonMultipathNexthop(nexthop));
      int bcm_port_id =
          nexthop.port_case() == BcmNonMultipathNexthop::kLogicalPort
              ? nexthop.logical_port()
              : nexthop.trunk_port();
      // Update the internal records in BcmTableManager. Note that if the
      // egress intf ID is already assigned to an existing member, this
      // method will return error. We keep a one-to-one map between members
      // and non-multipath egress intfs.
      RETURN_IF_ERROR(bcm_table_manager_->AddActionProfileMember(
          member, nexthop.type(), egress_intf_id, bcm_port_id));
      consumed = true;
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      // Member mod can happen even when the member is being referenced by flows
      // and/or groups. Member mod means keep the egress intf ID the same and
      // but modify the nexthop info of the egress intf.
      BcmNonMultipathNexthopInfo info;
      RETURN_IF_ERROR(bcm_table_manager_->GetBcmNonMultipathNexthopInfo(
          member_id, &info));  // will error out if member not found
      int egress_intf_id = info.egress_intf_id;  // egress intf id of the member
      // Fill up BcmNonMultipathNexthop for the member and add it to the HW.
      // Then check if adding to HW ends up creating a new egress intf.
      BcmNonMultipathNexthop nexthop;
      RETURN_IF_ERROR(
          bcm_table_manager_->FillBcmNonMultipathNexthop(member, &nexthop));
      CHECK_RETURN_IF_FALSE(unit_ == nexthop.unit())
          << "Something is wrong. This should never happen (" << unit_
          << " != " << nexthop.unit() << ").";
      RETURN_IF_ERROR(
          bcm_l3_manager_->ModifyNonMultipathNexthop(egress_intf_id, nexthop));
      int bcm_port_id =
          nexthop.port_case() == BcmNonMultipathNexthop::kLogicalPort
              ? nexthop.logical_port()
              : nexthop.trunk_port();
      // Update the internal records in BcmTableManager.
      RETURN_IF_ERROR(bcm_table_manager_->UpdateActionProfileMember(
          member, nexthop.type(), bcm_port_id));
      consumed = true;
      break;
    }
    case ::p4::v1::Update::DELETE: {
      // Removing a member which does not exist or is already being used by a
      // group or a flow (i.e. has non-zero ref count) is not allowed. If
      // member has not been used by any group or flow yet (i.e. has zero ref
      // count), we can safely remove it.
      BcmNonMultipathNexthopInfo info;
      RETURN_IF_ERROR(
          bcm_table_manager_->GetBcmNonMultipathNexthopInfo(member_id, &info));
      CHECK_RETURN_IF_FALSE(info.group_ref_count == 0 &&
                            info.flow_ref_count == 0)
          << "member_id " << member_id << " is already used by "
          << info.group_ref_count << " groups and " << info.flow_ref_count
          << "flows on node " << node_id_
          << ". ActionProfileMember: " << member.ShortDebugString() << ".";
      // Delete the member from HW.
      RETURN_IF_ERROR(
          bcm_l3_manager_->DeleteNonMultipathNexthop(info.egress_intf_id));
      // Update the internal records in BcmTableManager.
      RETURN_IF_ERROR(bcm_table_manager_->DeleteActionProfileMember(member));
      consumed = true;
      break;
    }
    default:
      break;
  }

  CHECK_RETURN_IF_FALSE(consumed)
      << "Do not know what to do with this ActionProfileMember: "
      << member.ShortDebugString() << ".";

  return ::util::OkStatus();
}

// TODO(max): complete implementation
::util::Status BcmNode::PacketReplicationEngineEntryWrite(
    const ::p4::v1::PacketReplicationEngineEntry& entry,
    ::p4::v1::Update::Type type) {
  auto replicationType = entry.type_case();

  bool consumed = false;
  switch (type) {
    case ::p4::v1::Update::INSERT: {
      switch (replicationType) {
        case ::p4::v1::PacketReplicationEngineEntry::TypeCase::
            kCloneSessionEntry: {
          BcmPacketReplicationEntry bcm_entry;
          RETURN_IF_ERROR(
              bcm_table_manager_->FillBcmReplicationConfig(entry, &bcm_entry));
          RETURN_IF_ERROR(
              bcm_table_manager_->AddCloneSession(entry.clone_session_entry()));
          // There is nothing to be done here. All packets cloned by COPY_TO_CPU
          // are sent to the CPU and then controller on bcm.
          consumed = true;
          break;
        }
        case ::p4::v1::PacketReplicationEngineEntry::TypeCase::
            kMulticastGroupEntry: {
          BcmPacketReplicationEntry bcm_entry;
          RETURN_IF_ERROR(
              bcm_table_manager_->FillBcmReplicationConfig(entry, &bcm_entry));
          RETURN_IF_ERROR(
              bcm_packetio_manager_->InsertPacketReplicationEntry(bcm_entry));
          RETURN_IF_ERROR(bcm_table_manager_->AddMulticastGroup(
              entry.multicast_group_entry()));
          consumed = true;
          break;
        }
        default:
          break;
      }
      break;
    }
    case ::p4::v1::Update::DELETE: {
      switch (replicationType) {
        case ::p4::v1::PacketReplicationEngineEntry::TypeCase::
            kCloneSessionEntry: {
          BcmPacketReplicationEntry bcm_entry;
          RETURN_IF_ERROR(
              bcm_table_manager_->FillBcmReplicationConfig(entry, &bcm_entry));
          RETURN_IF_ERROR(bcm_table_manager_->DeleteCloneSession(
              entry.clone_session_entry()));
          consumed = true;
          break;
        }
        case ::p4::v1::PacketReplicationEngineEntry::TypeCase::
            kMulticastGroupEntry: {
          BcmPacketReplicationEntry bcm_entry;
          RETURN_IF_ERROR(
              bcm_table_manager_->FillBcmReplicationConfig(entry, &bcm_entry));
          RETURN_IF_ERROR(
              bcm_packetio_manager_->DeletePacketReplicationEntry(bcm_entry));
          RETURN_IF_ERROR(bcm_table_manager_->DeleteMulticastGroup(
              entry.multicast_group_entry()));
          consumed = true;
          break;
        }
        default:
          break;
      }
      break;
    }
    default:
      break;
  }

  CHECK_RETURN_IF_FALSE(consumed)
      << "Do not know what to do with this "
      << ::p4::v1::Update::Type_Name(type)
      << " PacketReplicationEngineEntry: " << entry.ShortDebugString() << ".";
  return ::util::OkStatus();
}

namespace {

::util::Status CheckForUniqueMemberIds(
    const ::p4::v1::ActionProfileGroup& group) {
  std::set<uint32> member_ids = {};
  for (const auto& member : group.members()) {
    uint32 member_id = member.member_id();
    // We do not allow repeated member_ids as well. It does not make sense.
    // Controller can use the weights instead.
    CHECK_RETURN_IF_FALSE(!member_ids.count(member_id))
        << "member_id " << member_id << " is given more than once. "
        << "ActionProfileGroup: " << group.ShortDebugString() << ".";
    member_ids.insert(member_id);
  }

  return ::util::OkStatus();
}

}  // namespace

::util::Status BcmNode::ActionProfileGroupWrite(
    const ::p4::v1::ActionProfileGroup& group, ::p4::v1::Update::Type type) {
  bool consumed = false;  // will be set to true if we know what to do
  // Here, we only support ActionProfiles for ECMP/WCMP groups.
  uint32 group_id = group.group_id();
  switch (type) {
    case ::p4::v1::Update::INSERT: {
      // All the members that are being added to the group must exist. But the
      // group itself must not exist.
      CHECK_RETURN_IF_FALSE(
          !bcm_table_manager_->ActionProfileGroupExists(group_id))
          << "group_id " << group_id << " already exists on node " << node_id_
          << ". ActionProfileGroup: " << group.ShortDebugString() << ".";
      RETURN_IF_ERROR(CheckForUniqueMemberIds(group));
      // Find BcmMultipathNexthop for the group to be created.
      BcmMultipathNexthop nexthop;
      RETURN_IF_ERROR(bcm_table_manager_->FillBcmMultipathNexthop(
          group, &nexthop));  // will error out if any member not found
      ASSIGN_OR_RETURN(int egress_intf_id,
                       bcm_l3_manager_->FindOrCreateMultipathNexthop(nexthop));
      // Update the internal records in BcmTableManager. Note that if the
      // egress intf ID is already assigned to an existing group, this method
      // will return error.
      RETURN_IF_ERROR(
          bcm_table_manager_->AddActionProfileGroup(group, egress_intf_id));
      consumed = true;
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      // Group mod can happen even when the group is being referenced by flows.
      // Group mod is nothing but mutating the list of the members of an
      // existing group or the weights of the members. Note that all the new
      // members as well as the group itself must exist (the old members
      // already exist if any).
      BcmMultipathNexthopInfo info;
      RETURN_IF_ERROR(bcm_table_manager_->GetBcmMultipathNexthopInfo(
          group_id, &info));  // will error out if group not found
      int egress_intf_id = info.egress_intf_id;  // egress intf id of the group
      RETURN_IF_ERROR(CheckForUniqueMemberIds(group));
      // We now find a BcmMultipathNexthop containing the new members and try
      // to update the "existing" group with this new nexthop. Note that if
      // the new members and the old ones match, this call is a NOOP.
      BcmMultipathNexthop nexthop;
      RETURN_IF_ERROR(bcm_table_manager_->FillBcmMultipathNexthop(
          group, &nexthop));  // will error out if any member not found
      CHECK_RETURN_IF_FALSE(unit_ == nexthop.unit())
          << "Something is wrong. This should never happen (" << unit_
          << " != " << nexthop.unit() << ").";
      RETURN_IF_ERROR(
          bcm_l3_manager_->ModifyMultipathNexthop(egress_intf_id, nexthop));
      // Update the internal records in BcmTableManager. Note that there
      // is no change in the egress intf for the group so no need to pass
      // anything else to the function.
      RETURN_IF_ERROR(bcm_table_manager_->UpdateActionProfileGroup(group));
      consumed = true;
      break;
    }
    case ::p4::v1::Update::DELETE: {
      // Note that removing groups will not remove the members.
      BcmMultipathNexthopInfo info;
      RETURN_IF_ERROR(bcm_table_manager_->GetBcmMultipathNexthopInfo(
          group_id, &info));  // will error out if group not found
      CHECK_RETURN_IF_FALSE(info.flow_ref_count == 0)
          << "group_id " << group_id << " is already used by "
          << info.flow_ref_count << " flows on node " << node_id_
          << ". ActionProfileGroup: " << group.ShortDebugString() << ".";
      // Delete the group from hardware.
      RETURN_IF_ERROR(
          bcm_l3_manager_->DeleteMultipathNexthop(info.egress_intf_id));
      // Update the internal records in BcmTableManager.
      RETURN_IF_ERROR(bcm_table_manager_->DeleteActionProfileGroup(group));
      consumed = true;
      break;
    }
    default:
      break;
  }

  CHECK_RETURN_IF_FALSE(consumed)
      << "Do not know what to do with this ActionProfileGroup: "
      << group.ShortDebugString() << ".";

  return ::util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
