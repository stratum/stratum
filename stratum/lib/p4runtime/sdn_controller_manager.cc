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
    LOG(ERROR) << "Could not send arbitration update response to gRPC context '"
               << grpc_context_ << "': " << response.ShortDebugString();
  }
}

grpc::Status SdnControllerManager::HandleArbitrationUpdate(
    const p4::v1::MasterArbitrationUpdate& update, SdnConnection* controller) {
  absl::MutexLock l(&lock_);

  // If the role name is not set then we assume the connection is a 'root'
  // connection.
  absl::optional<std::string> role_name;
  if (update.has_role() && !update.role().name().empty()) {
    role_name = update.role().name();
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

  absl::optional<absl::uint128> election_id;
  if (request.has_election_id()) {
    election_id = absl::MakeUint128(request.election_id().high(),
                                    request.election_id().low());
  }
  return AllowRequest(role_name, election_id);
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
        found_at_least_one_primary = true;
        connection->SendStreamMessageResponse(response);
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
