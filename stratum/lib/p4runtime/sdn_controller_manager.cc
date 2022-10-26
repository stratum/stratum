// Copyright 2020 Google LLC
// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/p4runtime/sdn_controller_manager.h"

#include <algorithm>

#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/types/optional.h"
#include "glog/logging.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/hal/lib/p4/utils.h"

namespace stratum {
namespace p4runtime {
namespace {

std::string PrettyPrintRoleName(const absl::optional<std::string>& name) {
  return (name.has_value()) ? absl::StrCat("'", *name, "'") : "<default>";
}

std::string PrettyPrintElectionId(const absl::optional<absl::uint128>& id) {
  if (id.has_value()) {
    p4::v1::Uint128 p4_id;
    p4_id.set_high(absl::Uint128High64(*id));
    p4_id.set_low(absl::Uint128Low64(*id));
    return absl::StrCat("{ ", p4_id.ShortDebugString(), " }");
  }
  return "<backup>";
}

grpc::Status VerifyElectionIdIsUnused(
    const absl::optional<std::string>& role_name,
    const absl::optional<absl::uint128>& election_id,
    absl::Span<const SdnConnection* const> active_connections,
    SdnConnection const* current_connection) {
  // If the election ID is not set then the controller is saying this should be
  // a backup connection, and we allow any number of backup connections.
  if (!election_id.has_value()) return grpc::Status::OK;

  // Otherwise, we verify the election ID is unique among all active connections
  // for a given role (excluding the root role).
  for (auto* connection : active_connections) {
    if (connection == current_connection) continue;
    if (connection->GetRoleName() == role_name &&
        connection->GetElectionId() == election_id) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          absl::StrCat("Election ID ", PrettyPrintElectionId(election_id),
                       " is already used by another connection "
                       "with the same role."));
    }
  }
  return grpc::Status::OK;
}

grpc::Status VerifyElectionIdIsActive(
    const absl::optional<std::string>& role_name,
    const absl::optional<absl::uint128>& election_id,
    absl::Span<const SdnConnection* const> active_connections) {
  for (const auto& connection : active_connections) {
    if (connection->GetRoleName() == role_name &&
        connection->GetElectionId() == election_id) {
      return grpc::Status::OK;
    }
  }
  return grpc::Status(
      grpc::StatusCode::PERMISSION_DENIED,
      absl::StrCat("Election ID ", PrettyPrintElectionId(election_id),
                   " is not active for role ", PrettyPrintRoleName(role_name)));
}

grpc::Status VerifyRoleCanPushPipeline(
    const absl::optional<std::string>& role_name,
    const absl::flat_hash_map<absl::optional<std::string>,
                              absl::optional<P4RoleConfig>>& role_configs) {
  const auto& role_config = role_configs.find(role_name);
  if (role_config == role_configs.end()) {
    return grpc::Status(
        grpc::StatusCode::NOT_FOUND,
        absl::StrCat("Role ", PrettyPrintRoleName(role_name), " is unknown."));
  }
  if (!role_config->second.has_value()) return grpc::Status::OK;
  if (!role_config->second->can_push_pipeline()) {
    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                        absl::StrCat("Role ", PrettyPrintRoleName(role_name),
                                     " is not allowed to push pipelines."));
  }

  return grpc::Status::OK;
}

grpc::Status VerifyRoleConfig(
    const absl::optional<std::string>& role_name,
    const absl::optional<P4RoleConfig>& role_config,
    const absl::flat_hash_map<absl::optional<std::string>,
                              absl::optional<P4RoleConfig>>& existing_configs) {
  if (!role_config.has_value()) return grpc::Status::OK;

  // Verify that requested IDs are not exclusive to other roles already.
  for (const auto& e : existing_configs) {
    if (!e.second.has_value()) {
      continue;
    }
    // Don't compare role to itself.
    if (e.first == role_name) {
      continue;
    }
    // Ensure exclusive IDs do not overlap.
    std::vector<uint32_t> new_ids(role_config->exclusive_p4_ids().begin(),
                                  role_config->exclusive_p4_ids().end());
    std::vector<uint32_t> existing_ids(e.second->exclusive_p4_ids().begin(),
                                       e.second->exclusive_p4_ids().end());
    std::vector<uint32_t> common_ids;
    std::sort(new_ids.begin(), new_ids.end());
    std::sort(existing_ids.begin(), existing_ids.end());
    std::set_intersection(new_ids.begin(), new_ids.end(), existing_ids.begin(),
                          existing_ids.end(), std::back_inserter(common_ids));
    if (!common_ids.empty()) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          absl::StrCat("Role config ", PrettyPrintRoleName(role_name),
                       " contains exclusive IDs that overlap "
                       "with existing exclusive IDs."));
    }
    // Ensure new exclusive IDs and existing shared IDs do not overlap.
    std::vector<uint32_t> existing_shared_ids(e.second->shared_p4_ids().begin(),
                                              e.second->shared_p4_ids().end());
    std::sort(existing_shared_ids.begin(), existing_shared_ids.end());
    common_ids.clear();
    std::set_intersection(
        new_ids.begin(), new_ids.end(), existing_shared_ids.begin(),
        existing_shared_ids.end(), std::back_inserter(common_ids));
    if (!common_ids.empty()) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          absl::StrCat("Role config ", PrettyPrintRoleName(role_name),
                       " contains exclusive IDs that overlap "
                       "with existing shared IDs."));
    }
    // Ensure new shared IDs and existing exclusive IDs do not overlap.
    std::vector<uint32_t> new_shared_ids(role_config->shared_p4_ids().begin(),
                                         role_config->shared_p4_ids().end());
    std::sort(new_shared_ids.begin(), new_shared_ids.end());
    common_ids.clear();
    std::set_intersection(new_shared_ids.begin(), new_shared_ids.end(),
                          existing_ids.begin(), existing_ids.end(),
                          std::back_inserter(common_ids));
    if (!common_ids.empty()) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          absl::StrCat("Role config ", PrettyPrintRoleName(role_name),
                       " contains shared IDs that overlap "
                       "with existing exclusive IDs."));
    }
  }

  // Verify that PacketIns are enabled when a PacketIn filter is configured.
  if (!role_config->receives_packet_ins() &&
      role_config->has_packet_in_filter()) {
    return grpc::Status(
        grpc::StatusCode::INVALID_ARGUMENT,
        absl::StrCat("Role config ", PrettyPrintRoleName(role_name),
                     " contains a PacketIn filter, but disables "
                     "PacketIn delivery."));
  }

  // Verify that if a PacketIn filter is set, it must be non-empty.
  if (role_config->has_packet_in_filter() &&
      role_config->packet_in_filter().value().empty()) {
    return grpc::Status(
        grpc::StatusCode::INVALID_ARGUMENT,
        absl::StrCat("Role config ", PrettyPrintRoleName(role_name),
                     " contains an empty PacketIn filter."));
  }

  // TODO(max): verify packet filters for valid metadata

  return grpc::Status::OK;
}

bool VerifyStreamMessageNotFiltered(
    const absl::optional<P4RoleConfig>& role_config,
    const p4::v1::StreamMessageResponse& response) {
  if (!role_config.has_value()) return true;  // No filter rules set, allow.

  switch (response.update_case()) {
    case p4::v1::StreamMessageResponse::kPacket: {
      if (!role_config->receives_packet_ins()) return false;
      if (!role_config->has_packet_in_filter()) return true;
      for (const auto& metadata : response.packet().metadata()) {
        if (role_config->packet_in_filter().metadata_id() ==
                metadata.metadata_id() &&
            role_config->packet_in_filter().value() == metadata.value()) {
          return true;
        }
      }
      VLOG(1) << "Discarding PacketIn " << response.packet().ShortDebugString()
              << " because it did not match the role config filter: "
              << role_config->packet_in_filter().ShortDebugString() << ".";
      return false;  // No packet filter match, discard.
    }
    default:
      // TODO(max): implement filtering for other message types
      return true;
  }
}

uint32_t GetP4IdFromEntity(const p4::v1::Entity& entity) {
  switch (entity.entity_case()) {
    case p4::v1::Entity::kExternEntry:
      return entity.extern_entry().extern_id();
    case p4::v1::Entity::kTableEntry:
      return entity.table_entry().table_id();
    case p4::v1::Entity::kActionProfileMember:
      return entity.action_profile_member().action_profile_id();
    case p4::v1::Entity::kActionProfileGroup:
      return entity.action_profile_group().action_profile_id();
    case p4::v1::Entity::kMeterEntry:
      return entity.meter_entry().meter_id();
    case p4::v1::Entity::kDirectMeterEntry:
      return entity.direct_meter_entry().table_entry().table_id();
    case p4::v1::Entity::kCounterEntry:
      return entity.counter_entry().counter_id();
    case p4::v1::Entity::kDirectCounterEntry:
      return entity.direct_counter_entry().table_entry().table_id();
    case p4::v1::Entity::kPacketReplicationEngineEntry:
      // PREs don't reference any P4 entity. Return without error.
      return 0;
    case p4::v1::Entity::kValueSetEntry:
      return entity.value_set_entry().value_set_id();
    case p4::v1::Entity::kRegisterEntry:
      return entity.register_entry().register_id();
    case p4::v1::Entity::kDigestEntry:
      return entity.digest_entry().digest_id();
    default:
      LOG(WARNING) << "Unsupported entity type: " << entity.ShortDebugString();
      return 0;
  }
}

std::vector<uint32_t> GetP4IdsFromRequest(const p4::v1::ReadRequest& request) {
  std::vector<uint32_t> ids;
  for (const auto& entity : request.entities()) {
    uint32_t id = GetP4IdFromEntity(entity);
    if (id) ids.push_back(id);
  }

  return ids;
}

std::vector<uint32_t> GetP4IdsFromRequest(const p4::v1::WriteRequest& request) {
  std::vector<uint32_t> ids;
  for (const auto& update : request.updates()) {
    uint32_t id = GetP4IdFromEntity(update.entity());
    if (id) ids.push_back(id);
  }

  return ids;
}

grpc::Status VerifyRoleCanAccessIds(
    const absl::optional<std::string>& role_name,
    const std::vector<uint32_t>& ids,
    const absl::flat_hash_map<absl::optional<std::string>,
                              absl::optional<P4RoleConfig>>& role_configs) {
  const auto& role_config = role_configs.find(role_name);
  if (role_config == role_configs.end()) {
    return grpc::Status(
        grpc::StatusCode::NOT_FOUND,
        absl::StrCat("Role ", PrettyPrintRoleName(role_name), " is unknown."));
  }
  if (!role_config->second.has_value()) return grpc::Status::OK;
  VLOG(1) << "Testing IDs against role config: "
          << role_config->second->ShortDebugString();
  for (const auto& id : ids) {
    if (id == 0) continue;
    if (std::find(role_config->second->exclusive_p4_ids().begin(),
                  role_config->second->exclusive_p4_ids().end(),
                  id) != role_config->second->exclusive_p4_ids().end()) {
      continue;
    }
    if (std::find(role_config->second->shared_p4_ids().begin(),
                  role_config->second->shared_p4_ids().end(),
                  id) != role_config->second->shared_p4_ids().end()) {
      continue;
    }
    VLOG(1) << "Role " << PrettyPrintRoleName(role_name)
            << " not allowed to access entity with ID " << id << ".";
    return grpc::Status(
        grpc::StatusCode::PERMISSION_DENIED,
        absl::StrCat("Role ", PrettyPrintRoleName(role_name),
                     " is not allowed to access entity with ID ", id, "."));
  }

  return grpc::Status::OK;
}

}  // namespace

void SdnConnection::SetElectionId(const absl::optional<absl::uint128>& id) {
  election_id_ = id;
}

absl::optional<absl::uint128> SdnConnection::GetElectionId() const {
  return election_id_;
}

void SdnConnection::SetRoleName(const absl::optional<std::string>& name) {
  role_name_ = name;
}

absl::optional<std::string> SdnConnection::GetRoleName() const {
  return role_name_;
}

std::string SdnConnection::GetName() const {
  return absl::StrCat("(role_name: ", PrettyPrintRoleName(role_name_),
                      ", election_id: ", PrettyPrintElectionId(election_id_),
                      ", uri: ", grpc_context_->peer(), ")");
}

void SdnConnection::SendStreamMessageResponse(
    const p4::v1::StreamMessageResponse& response) {
  VLOG(2) << "Sending response: " << response.ShortDebugString();
  if (!grpc_stream_->Write(response)) {
    LOG(ERROR) << "Could not send stream message response to gRPC context '"
               << grpc_context_ << "': " << response.ShortDebugString();
  }
}

grpc::Status SdnControllerManager::HandleArbitrationUpdate(
    const p4::v1::MasterArbitrationUpdate& update, SdnConnection* controller) {
  absl::MutexLock l(&lock_);

  // If the role name is not set then we assume the connection is a 'root'
  // connection.
  absl::optional<std::string> role_name;
  absl::optional<P4RoleConfig> role_config;
  if (update.has_role() && !update.role().name().empty()) {
    role_name = update.role().name();
  }

  if (update.has_role() && update.role().has_config()) {
    P4RoleConfig rc;
    if (!update.role().config().UnpackTo(&rc)) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "Unknown role config format.");
    }
    // Ensure packet filter byte string is canonical for later comparison.
    if (rc.has_packet_in_filter()) {
      rc.mutable_packet_in_filter()->set_value(
          hal::ByteStringToP4RuntimeByteString(rc.packet_in_filter().value()));
    }

    role_config = rc;
  }

  // Validate the role config.
  grpc::Status status =
      VerifyRoleConfig(role_name, role_config, role_config_by_name_);
  if (!status.ok()) {
    return status;
  }

  if (!role_name.has_value() && role_config.has_value()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Cannot set a role config for the default role.");
  }

  const auto old_election_id_for_connection = controller->GetElectionId();
  absl::optional<absl::uint128> new_election_id_for_connection;
  if (update.has_election_id()) {
    new_election_id_for_connection = absl::MakeUint128(
        update.election_id().high(), update.election_id().low());
  }

  const bool new_connection = !controller->IsInitialized();

  if (new_connection) {
    // First arbitration message sent by this controller.

    // Verify the request's device ID is being sent to the correct device.
    if (update.device_id() != device_id_) {
      return grpc::Status(
          grpc::StatusCode::NOT_FOUND,
          absl::StrCat("Arbitration request has the wrong device ID '",
                       update.device_id(),
                       "'. Cannot establish connection to this device '",
                       device_id_, "'."));
    }

    // Check if the election ID is being use by another connection.
    auto election_id_is_unused = VerifyElectionIdIsUnused(
        role_name, new_election_id_for_connection, connections_, controller);
    if (!election_id_is_unused.ok()) {
      return election_id_is_unused;
    }

    controller->SetRoleName(role_name);
    controller->SetElectionId(new_election_id_for_connection);
    controller->Initialize();
    connections_.push_back(controller);
    LOG(INFO) << "New SDN connection " << controller->GetName() << ": "
              << update.ShortDebugString();
  } else {
    // Update arbitration message sent from the controller.

    // The device ID cannot change.
    if (update.device_id() != device_id_) {
      return grpc::Status(
          grpc::StatusCode::FAILED_PRECONDITION,
          absl::StrCat("Arbitration request cannot change the device ID from '",
                       device_id_, "' to '", update.device_id(), "'."));
    }

    // The role cannot change without closing the connection.
    if (role_name != controller->GetRoleName()) {
      return grpc::Status(
          grpc::StatusCode::FAILED_PRECONDITION,
          absl::StrCat("Arbitration request cannot change the role from ",
                       PrettyPrintRoleName(controller->GetRoleName()), " to ",
                       PrettyPrintRoleName(role_name), "."));
    }

    // Check if the election ID is being use by another connection.
    auto election_id_is_unused = VerifyElectionIdIsUnused(
        role_name, new_election_id_for_connection, connections_, controller);
    if (!election_id_is_unused.ok()) {
      return election_id_is_unused;
    }
    controller->SetElectionId(new_election_id_for_connection);

    LOG(INFO) << absl::StreamFormat("Update SDN connection %s: %s",
                                    controller->GetName(),
                                    update.ShortDebugString());
  }

  // Check for any primary connection changes, and inform all active connections
  // as needed.
  auto& election_id_past_for_role = election_id_past_by_role_[role_name];
  const bool connection_was_primary =
      old_election_id_for_connection.has_value() &&
      old_election_id_for_connection == election_id_past_for_role;
  const bool connection_is_new_primary =
      new_election_id_for_connection.has_value() &&
      (!election_id_past_for_role.has_value() ||
       *new_election_id_for_connection >= *election_id_past_for_role);

  if (connection_is_new_primary) {
    election_id_past_for_role = new_election_id_for_connection;
    // Update the configuration for this controllers role.
    role_config_by_name_[role_name] = role_config;
    // The spec demands we send a notifcation even if the old & new primary
    // match.
    InformConnectionsAboutPrimaryChange(role_name);
    LOG(INFO) << (connection_was_primary ? "Old and new " : "New ")
              << "primary connection for role "
              << PrettyPrintRoleName(role_name) << " with election ID "
              << PrettyPrintElectionId(*new_election_id_for_connection) << ".";
    // If there was a previous primary, we need to ensure write requests by the
    // old primary and new primary are not interleaved, and the spec carefully
    // specifies how to do this.
    // Our implementation simply rules out all interleavings by using a common
    // lock, so no special handling is needed here.
  } else {
    if (connection_was_primary) {
      // This connection was previously the primary and downgrades to backup.
      InformConnectionsAboutPrimaryChange(role_name);
      LOG(INFO) << "Primary connection for role "
                << PrettyPrintRoleName(role_name)
                << " is downgrading to backup with election ID "
                << PrettyPrintElectionId(new_election_id_for_connection)
                << "; no longer have a primary.";
    } else {
      SendArbitrationResponse(controller);
      LOG(INFO) << "Backup connection for role "
                << PrettyPrintRoleName(role_name) << " with "
                << (new_connection ? "initial " : "changed ") << "election ID "
                << PrettyPrintElectionId(new_election_id_for_connection);
    }
  }

  return grpc::Status::OK;
}

void SdnControllerManager::Disconnect(SdnConnection* connection) {
  absl::MutexLock l(&lock_);

  // If the connection was never initialized then there is no work needed to
  // disconnect it.
  if (!connection->IsInitialized()) return;

  bool was_primary = connection->GetElectionId().has_value() &&
                     (connection->GetElectionId() ==
                      election_id_past_by_role_[connection->GetRoleName()]);

  // Iterate through the list connections and remove this connection.
  for (auto iter = connections_.begin(); iter != connections_.end(); ++iter) {
    if (*iter == connection) {
      LOG(INFO) << "Dropping " << (was_primary ? "primary" : "backup")
                << " SDN connection for role "
                << PrettyPrintRoleName(connection->GetRoleName())
                << " with election ID "
                << PrettyPrintElectionId(connection->GetElectionId()) << ".";
      connections_.erase(iter);
      break;
    }
  }

  // If connection was the primary connection we need to inform all existing
  // connections.
  if (was_primary) {
    InformConnectionsAboutPrimaryChange(connection->GetRoleName());
  }
}

grpc::Status SdnControllerManager::AllowRequest(
    const absl::optional<std::string>& role_name,
    const absl::optional<absl::uint128>& election_id) const {
  absl::MutexLock l(&lock_);

  if (!election_id.has_value()) {
    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                        "Request does not have an election ID.");
  }

  const auto& election_id_past_for_role =
      election_id_past_by_role_.find(role_name);
  if (election_id_past_for_role == election_id_past_by_role_.end()) {
    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                        "Only the primary connection can issue requests, but "
                        "no primary connection has been established.");
  }

  if (election_id != election_id_past_for_role->second) {
    return grpc::Status(
        grpc::StatusCode::PERMISSION_DENIED,
        absl::StrCat("Only the primary connection can issue requests, but this "
                     "SDN connection for role ",
                     PrettyPrintRoleName(role_name), " with election ID ",
                     PrettyPrintElectionId(election_id), " is not primary."));
  }

  return VerifyElectionIdIsActive(role_name, election_id, connections_);
}

grpc::Status SdnControllerManager::AllowRequest(
    const p4::v1::WriteRequest& request) const {
  absl::optional<std::string> role_name;
  if (!request.role().empty()) {
    role_name = request.role();
  }

  if (role_name) {
    absl::MutexLock l(&lock_);
    grpc::Status status = VerifyRoleCanAccessIds(
        role_name, GetP4IdsFromRequest(request), role_config_by_name_);
    if (!status.ok()) {
      return status;
    }
  }

  absl::optional<absl::uint128> election_id;
  if (request.has_election_id()) {
    election_id = absl::MakeUint128(request.election_id().high(),
                                    request.election_id().low());
  }
  return AllowRequest(role_name, election_id);
}

grpc::Status SdnControllerManager::AllowRequest(
    const p4::v1::ReadRequest& request) const {
  absl::optional<std::string> role_name;
  if (!request.role().empty()) {
    role_name = request.role();
  }

  if (role_name) {
    absl::MutexLock l(&lock_);
    grpc::Status status = VerifyRoleCanAccessIds(
        role_name, GetP4IdsFromRequest(request), role_config_by_name_);
    if (!status.ok()) {
      return status;
    }
  }

  return grpc::Status::OK;
}

grpc::Status SdnControllerManager::AllowRequest(
    const p4::v1::SetForwardingPipelineConfigRequest& request) const {
  absl::optional<std::string> role_name;
  if (!request.role().empty()) {
    role_name = request.role();
  }

  {
    absl::MutexLock l(&lock_);
    grpc::Status status =
        VerifyRoleCanPushPipeline(role_name, role_config_by_name_);
    if (!status.ok()) {
      return status;
    }
  }

  absl::optional<absl::uint128> election_id;
  if (request.has_election_id()) {
    election_id = absl::MakeUint128(request.election_id().high(),
                                    request.election_id().low());
  }
  return AllowRequest(role_name, election_id);
}

int SdnControllerManager::ActiveConnections() const {
  absl::MutexLock l(&lock_);
  return connections_.size();
}

p4::v1::ReadRequest SdnControllerManager::ExpandWildcardsInReadRequest(
    const p4::v1::ReadRequest& request,
    const p4::config::v1::P4Info& p4info) const {
  absl::MutexLock l(&lock_);

  absl::optional<std::string> role_name;
  if (!request.role().empty()) {
    role_name = request.role();
  }

  // Copy the request, except for the entities.
  p4::v1::ReadRequest ret = request;
  ret.clear_entities();

  // Next, expand wildcard reads into individual table reads.
  for (const auto& entity : request.entities()) {
    // Check if wildcard or single read. Single reads are preserved as is.
    uint32_t id = GetP4IdFromEntity(entity);
    if (id != 0) {
      *ret.add_entities() = entity;
      continue;
    }

    // Expand the wildcard entities into a series of single entity reads.
    switch (entity.entity_case()) {
      case p4::v1::Entity::kTableEntry: {
        for (const auto& table : p4info.tables()) {
          if (VerifyRoleCanAccessIds(role_name, {table.preamble().id()},
                                     role_config_by_name_)
                  .ok()) {
            p4::v1::Entity table_entry = entity;
            table_entry.mutable_table_entry()->set_table_id(
                table.preamble().id());
            *ret.add_entities() = table_entry;
          }
        }
        break;
      }
      case p4::v1::Entity::kCounterEntry: {
        for (const auto& counter : p4info.counters()) {
          if (VerifyRoleCanAccessIds(role_name, {counter.preamble().id()},
                                     role_config_by_name_)
                  .ok()) {
            p4::v1::Entity counter_entry = entity;
            counter_entry.mutable_counter_entry()->set_counter_id(
                counter.preamble().id());
            *ret.add_entities() = counter_entry;
          }
        }
        break;
      }
      case p4::v1::Entity::kMeterEntry: {
        for (const auto& meter : p4info.meters()) {
          if (VerifyRoleCanAccessIds(role_name, {meter.preamble().id()},
                                     role_config_by_name_)
                  .ok()) {
            p4::v1::Entity meter_entry = entity;
            meter_entry.mutable_meter_entry()->set_meter_id(
                meter.preamble().id());
            *ret.add_entities() = meter_entry;
          }
        }
        break;
      }
      case p4::v1::Entity::kRegisterEntry: {
        for (const auto& reg : p4info.registers()) {
          if (VerifyRoleCanAccessIds(role_name, {reg.preamble().id()},
                                     role_config_by_name_)
                  .ok()) {
            p4::v1::Entity register_entry = entity;
            register_entry.mutable_register_entry()->set_register_id(
                reg.preamble().id());
            *ret.add_entities() = register_entry;
          }
        }
        break;
      }
      default: {
        VLOG(1) << "Expanding entity " << entity.ShortDebugString()
                << " not supported yet.";
        *ret.add_entities() = entity;
        break;
      }
    }
  }

  return ret;
}

void SdnControllerManager::InformConnectionsAboutPrimaryChange(
    const absl::optional<std::string>& role_name) {
  VLOG(1) << "Informing all connections about primary connection change.";
  for (const auto& connection : connections_) {
    if (connection->GetRoleName() == role_name) {
      SendArbitrationResponse(connection);
    }
  }
}

bool SdnControllerManager::PrimaryConnectionExists(
    const absl::optional<std::string>& role_name) {
  absl::optional<absl::uint128> election_id_past_for_role =
      election_id_past_by_role_[role_name];

  for (const auto& connection : connections_) {
    if (connection->GetRoleName() == role_name &&
        connection->GetElectionId() == election_id_past_for_role) {
      return election_id_past_for_role.has_value();
    }
  }
  return false;
}

void SdnControllerManager::SendArbitrationResponse(SdnConnection* connection) {
  p4::v1::StreamMessageResponse response;
  auto arbitration = response.mutable_arbitration();

  // Always set device ID.
  arbitration->set_device_id(device_id_);

  // Populate the role only if the connection has set one.
  if (connection->GetRoleName().has_value()) {
    *arbitration->mutable_role()->mutable_name() =
        connection->GetRoleName().value();
    absl::optional<P4RoleConfig> role_config =
        role_config_by_name_[connection->GetRoleName()];
    if (role_config.has_value()) {
      arbitration->mutable_role()->mutable_config()->PackFrom(*role_config);
    }
  }

  // Populate the election ID with the highest accepted value.
  absl::optional<absl::uint128> election_id_past_for_role =
      election_id_past_by_role_[connection->GetRoleName()];
  if (election_id_past_for_role.has_value()) {
    arbitration->mutable_election_id()->set_high(
        absl::Uint128High64(*election_id_past_for_role));
    arbitration->mutable_election_id()->set_low(
        absl::Uint128Low64(*election_id_past_for_role));
  }

  // Update connection status for the arbitration response.
  auto status = arbitration->mutable_status();
  if (PrimaryConnectionExists(connection->GetRoleName())) {
    // has primary connection.
    if (election_id_past_for_role == connection->GetElectionId()) {
      // and this connection is it.
      status->set_code(grpc::StatusCode::OK);
      status->set_message("you are the primary connection.");
    } else {
      // but this connection is a backup.
      status->set_code(grpc::StatusCode::ALREADY_EXISTS);
      status->set_message(
          "you are a backup connection, and a primary connection exists.");
    }
  } else {
    // no primary connection exists.
    status->set_code(grpc::StatusCode::NOT_FOUND);
    status->set_message(
        "you are a backup connection, and NO primary connection exists.");
  }

  connection->SendStreamMessageResponse(response);
}

absl::Status SdnControllerManager::SendPacketInToPrimary(
    const p4::v1::StreamMessageResponse& response) {
  if (response.update_case() != p4::v1::StreamMessageResponse::kPacket) {
    LOG(WARNING) << "PacketIn stream message update has to be a packet: "
                 << response.DebugString();
    return absl::InvalidArgumentError("PacketIn message must use a packet.");
  }
  return SendStreamMessageToPrimary(response);
}

absl::Status SdnControllerManager::SendStreamMessageToPrimary(
    const p4::v1::StreamMessageResponse& response) {
  absl::MutexLock l(&lock_);

  bool found_at_least_one_primary = false;

  for (const auto& connection : connections_) {
    absl::optional<absl::uint128> election_id_past_for_role =
        election_id_past_by_role_[connection->GetRoleName()];
    if (election_id_past_for_role.has_value() &&
        election_id_past_for_role == connection->GetElectionId()) {
      absl::optional<P4RoleConfig> role_config =
          role_config_by_name_[connection->GetRoleName()];
      if (VerifyStreamMessageNotFiltered(role_config, response)) {
        found_at_least_one_primary = true;
        connection->SendStreamMessageResponse(response);
      }
      // We don't report an error for packets getting filtered as this is
      // expected operation.
    }
  }

  if (!found_at_least_one_primary) {
    LOG(WARNING) << "Cannot send stream message response because there is no "
                 << "active primary connection: " << response.DebugString();
    return absl::FailedPreconditionError(
        "No active role has a primary connection configured to receive "
        "StreamMessageResponse messages.");
  }
  return absl::OkStatus();
}

}  // namespace p4runtime
}  // namespace stratum
