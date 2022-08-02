// Copyright 2020 Google LLC
// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/p4runtime/sdn_controller_manager.h"

#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/types/optional.h"
#include "glog/logging.h"
#include "p4/v1/p4runtime.pb.h"

namespace stratum {
namespace p4runtime {
namespace {

std::string PrettyPrintRoleName(const absl::optional<std::string>& name) {
  return (name.has_value()) ? absl::StrCat("'", *name, "'") : "<default>";
}

std::string PrettyPrintRoleConfig(const absl::optional<P4RoleConfig>& config) {
  return (config.has_value())
             ? absl::StrCat("'", config->ShortDebugString(), "'")
             : "<default>";
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
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "Election ID is already used by another connection "
                          "with the same role.");
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
  return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                      "Election ID is not active for the role.");
}

uint32_t GetP4IdFromEntity(const ::p4::v1::Entity& entity) {
  switch (entity.entity_case()) {
    case ::p4::v1::Entity::kTableEntry:
      return entity.table_entry().table_id();
    case ::p4::v1::Entity::kExternEntry:
      return entity.extern_entry().extern_id();
    case ::p4::v1::Entity::kActionProfileMember:
      return entity.action_profile_member().action_profile_id();
    case ::p4::v1::Entity::kActionProfileGroup:
      return entity.action_profile_group().action_profile_id();
    // case ::p4::v1::Entity::kPacketReplicationEngineEntry:
    //       return
    //       entity.packet_replication_engine_entry().clone_session_entry().id;
    // case ::p4::v1::Entity::kDirectCounterEntry:
    //   return entity.direct_counter_entry().table_entry().table_id();
    case ::p4::v1::Entity::kCounterEntry:
      return entity.counter_entry().counter_id();
    case ::p4::v1::Entity::kRegisterEntry:
      return entity.register_entry().register_id();
    case ::p4::v1::Entity::kMeterEntry:
      return entity.meter_entry().meter_id();
    case ::p4::v1::Entity::kDirectMeterEntry:
    case ::p4::v1::Entity::kValueSetEntry:
    case ::p4::v1::Entity::kDigestEntry:
    default:
      LOG(ERROR) << "Unsupported entity type: " << entity.ShortDebugString();
      return 0;
  }
}

grpc::Status VerifyP4IdsArePermitted(
    const absl::optional<P4RoleConfig>& role_config,
    const ::p4::v1::WriteRequest& request) {
  // TODO(max): is empty role config an error?
  if (!role_config.has_value()) return grpc::Status::OK;

  for (const auto& update : request.updates()) {
    uint32_t id = GetP4IdFromEntity(update.entity());
    if (std::find(role_config->exclusive_p4_ids().begin(),
                  role_config->exclusive_p4_ids().end(),
                  id) != role_config->exclusive_p4_ids().end()) {
      continue;
    }
    if (std::find(role_config->shared_p4_ids().begin(),
                  role_config->shared_p4_ids().end(),
                  id) != role_config->shared_p4_ids().end()) {
      continue;
    }
    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                        "Role is not allowed to access this entity.");
  }

  return grpc::Status::OK;
}

grpc::Status VerifyP4IdsArePermitted(
    const absl::optional<P4RoleConfig>& role_config,
    const ::p4::v1::ReadRequest& request) {
  // TODO(max): is empty role config an error?
  if (!role_config.has_value()) return grpc::Status::OK;

  for (const auto& entity : request.entities()) {
    uint32_t id = GetP4IdFromEntity(entity);
    if (std::find(role_config->exclusive_p4_ids().begin(),
                  role_config->exclusive_p4_ids().end(),
                  id) != role_config->exclusive_p4_ids().end()) {
      continue;
    }
    if (std::find(role_config->shared_p4_ids().begin(),
                  role_config->shared_p4_ids().end(),
                  id) != role_config->shared_p4_ids().end()) {
      continue;
    }
    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                        "Role is not allowed to access this entity.");
  }

  return grpc::Status::OK;
}

grpc::Status VerifyRoleReceviesPacketIn(
    const absl::optional<P4RoleConfig>& role_config,
    const p4::v1::StreamMessageResponse& response) {
  if (!response.has_packet()) {
    return grpc::Status::OK;
  }

  // An empty role config implies full access.
  if (!role_config.has_value()) {
    return grpc::Status::OK;
  }

  for (const auto& metadata : response.packet().metadata()) {
    if (metadata.metadata_id() ==
            role_config.value().packet_in_filter().metadata_id() &&
        metadata.value() == role_config.value().packet_in_filter().value()) {
      return grpc::Status::OK;
    }
  }

  return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                      "Role is not allowed to receive this packet.");
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

  LOG(WARNING) << update.role().ShortDebugString();
  if (update.has_role() && update.role().has_config()) {
    LOG(WARNING) << "have role config: "
                 << update.role().config().ShortDebugString();
    P4RoleConfig rc;
    if (!update.role().config().UnpackTo(&rc)) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          absl::StrCat("Unknown role config."));
    }
    role_config = rc;
  }

  if (!role_name.has_value() && role_config.has_value()) {
    return grpc::Status(
        grpc::StatusCode::INVALID_ARGUMENT,
        absl::StrCat("Cannot set a role config for the default role."));
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

    LOG(INFO) << absl::StreamFormat("Update SDN connection (%s): %s",
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
    if (role_name.has_value() && role_config.has_value()) {
      if (role_config.value().receives_packet_ins()) {
        role_receives_packet_in_.insert(role_name);
      } else {
        role_receives_packet_in_.erase(role_name);
      }
    }
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
    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                        "Only the primary connection can issue requests.");
  }

  return VerifyElectionIdIsActive(role_name, election_id, connections_);
}

grpc::Status SdnControllerManager::AllowRequest(
    const p4::v1::WriteRequest& request) const {
  absl::optional<std::string> role_name;
  if (!request.role().empty()) {
    role_name = request.role();
  }

  {
    absl::MutexLock l(&lock_);
    const auto& role_config = role_config_by_name_.find(role_name);
    if (role_config == role_config_by_name_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND,
                          "Found no config for given role.");
    }
    grpc::Status result = VerifyP4IdsArePermitted(role_config->second, request);
    if (!result.ok()) {
      return result;
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

  // The default role is unrestricted.
  if (!role_name) {
    return grpc::Status::OK;
  }

  absl::MutexLock l(&lock_);
  if (!role_config_by_name_.contains(role_name)) {
    // We don't have the role config for this role, which can happen when no
    // master has connected yet.
    return grpc::Status::OK;
  }

  return VerifyP4IdsArePermitted(role_config_by_name_.at(role_name), request);
}

grpc::Status SdnControllerManager::AllowRequest(
    const p4::v1::SetForwardingPipelineConfigRequest& request) const {
  absl::optional<std::string> role_name;
  if (!request.role().empty()) {
    role_name = request.role();
  }

  absl::optional<absl::uint128> election_id;
  if (request.has_election_id()) {
    election_id = absl::MakeUint128(request.election_id().high(),
                                    request.election_id().low());
  }
  return AllowRequest(role_name, election_id);
}

p4::v1::ReadResponse SdnControllerManager::FilterReadResponse(
    const absl::optional<std::string>& role_name,
    const p4::v1::ReadResponse& response) const {
  absl::MutexLock l(&lock_);
  p4::v1::ReadResponse filtered_response = response;
  filtered_response.clear_entities();
  const auto& role_config = role_config_by_name_.find(role_name);
  // TODO(max): is empty role config an error?
  if (role_config == role_config_by_name_.end() ||
      !role_config->second.has_value()) {
    LOG(INFO) << "No role with name " << PrettyPrintRoleName(role_name)
              << " found.";
    return response;
  }

  for (const auto& entity : response.entities()) {
    uint32_t id = GetP4IdFromEntity(entity);
    if (std::find(role_config->second->exclusive_p4_ids().begin(),
                  role_config->second->exclusive_p4_ids().end(),
                  id) != role_config->second->exclusive_p4_ids().end()) {
      *filtered_response.add_entities() = entity;
    }
    if (std::find(role_config->second->shared_p4_ids().begin(),
                  role_config->second->shared_p4_ids().end(),
                  id) != role_config->second->shared_p4_ids().end()) {
      *filtered_response.add_entities() = entity;
    }
  }

  return filtered_response;
}

int SdnControllerManager::ActiveConnections() const {
  absl::MutexLock l(&lock_);
  return connections_.size();
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
    if (role_config_by_name_.contains(connection->GetRoleName()) &&
        role_config_by_name_.at(connection->GetRoleName()).has_value()) {
      arbitration->mutable_role()->mutable_config()->PackFrom(
          role_config_by_name_.at(connection->GetRoleName()).value());
    }
  }

  // Populate the election ID with the highest accepted value.
  absl::optional<absl::uint128> election_id_past_for_role =
      election_id_past_by_role_[connection->GetRoleName()];
  if (election_id_past_for_role.has_value()) {
    arbitration->mutable_election_id()->set_high(
        absl::Uint128High64(election_id_past_for_role.value()));
    arbitration->mutable_election_id()->set_low(
        absl::Uint128Low64(election_id_past_for_role.value()));
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
      if (role_receives_packet_in_.contains(connection->GetRoleName())) {
        auto role_config = role_config_by_name_.find(connection->GetRoleName());
        if (role_config == role_config_by_name_.end() ||
            VerifyRoleReceviesPacketIn(role_config->second, response).ok()) {
          found_at_least_one_primary = true;
          connection->SendStreamMessageResponse(response);
        }
        // else {
        //   if (VerifyRoleReceviesPacketIn(role_config->second, response).ok()) {
        //     found_at_least_one_primary = true;
        //     connection->SendStreamMessageResponse(response);
        //   }
        // }

        // if (!role_config_by_name_.contains(connection->GetRoleName())) {
        //     found_at_least_one_primary = true;
        //     connection->SendStreamMessageResponse(response);
        // }

        // if (!role_config_by_name_.contains(connection->GetRoleName()) ||
        //     (role_config_by_name_.contains(connection->GetRoleName()) &&
        //      VerifyRoleReceviesPacketIn(
        //          role_config_by_name_.at(connection->GetRoleName()),
        //          response) .ok())) {
        //   LOG(WARNING) << "Role " << connection->GetRoleName();
        //   LOG(WARNING) << "Role config "
        //                << role_config_by_name_.at(connection->GetRoleName());
        //   found_at_least_one_primary = true;
        //   connection->SendStreamMessageResponse(response);
        // }
      }
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
