// Copyright 2020 Google LLC
// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_P4RUNTIME_SDN_CONTROLLER_MANAGER_H_
#define STRATUM_LIB_P4RUNTIME_SDN_CONTROLLER_MANAGER_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/public/proto/p4_role_config.pb.h"

namespace stratum {
namespace p4runtime {

// Named role for a SDN controller.
constexpr char kP4RuntimeRoleSdnController[] = "sdn_controller";

// A connection between a controller and p4rt server.
class SdnConnection {
 public:
  SdnConnection(
      grpc::ServerContext* context,
      grpc::ServerReaderWriterInterface<p4::v1::StreamMessageResponse,
                                        p4::v1::StreamMessageRequest>* stream)
      : initialized_(false), grpc_context_(context), grpc_stream_(stream) {}

  void Initialize() { initialized_ = true; }
  bool IsInitialized() const { return initialized_; }

  void SetElectionId(const absl::optional<absl::uint128>& id);
  absl::optional<absl::uint128> GetElectionId() const;

  void SetRoleName(const absl::optional<std::string>& name);
  absl::optional<std::string> GetRoleName() const;

  // A unique name string for the controller.
  std::string GetName() const;

  // Sends back StreamMessageResponse to this controller.
  void SendStreamMessageResponse(const p4::v1::StreamMessageResponse& response);

 private:
  // The SDN connection should be initialized through arbitration before it can
  // be used.
  bool initialized_;

  // SDN connections are made to the P4RT gRPC service based on role types. The
  // specified role limits the table a connection can write to, and read from.
  // If no role is specified then the connection is assumed to be root, and has
  // access to all tables.
  absl::optional<std::string> role_name_;

  // Multiple connections can be established per role, but only one connection
  // (i.e. the primary connection) is allowed to modify state. The primary
  // connection is determined based on the election ID.
  absl::optional<absl::uint128> election_id_;

  // While the gRPC connection is open we keep access to the context & the
  // read/write stream for communication.
  grpc::ServerContext* grpc_context_;  // not owned.
  grpc::ServerReaderWriterInterface<p4::v1::StreamMessageResponse,
                                    p4::v1::StreamMessageRequest>*
      grpc_stream_;  // not owned.
};

class SdnControllerManager {
 public:
  explicit SdnControllerManager(uint64_t device_id) : device_id_(device_id) {}

  grpc::Status HandleArbitrationUpdate(
      const p4::v1::MasterArbitrationUpdate& update, SdnConnection* controller)
      ABSL_LOCKS_EXCLUDED(lock_);

  void Disconnect(SdnConnection* connection) ABSL_LOCKS_EXCLUDED(lock_);

  grpc::Status AllowRequest(const absl::optional<std::string>& role_name,
                            const absl::optional<absl::uint128>& election_id)
      const ABSL_LOCKS_EXCLUDED(lock_);
  grpc::Status AllowRequest(const p4::v1::WriteRequest& request) const
      ABSL_LOCKS_EXCLUDED(lock_);
  grpc::Status AllowRequest(const p4::v1::ReadRequest& request) const
      ABSL_LOCKS_EXCLUDED(lock_);
  grpc::Status AllowRequest(
      const p4::v1::SetForwardingPipelineConfigRequest& request) const
      ABSL_LOCKS_EXCLUDED(lock_);

  // Returns the number of currently active connections.
  int ActiveConnections() const ABSL_LOCKS_EXCLUDED(lock_);

  // Expands a generic wildcard request into individual entity wildcard reads.
  p4::v1::ReadRequest ExpandWildcardsInReadRequest(
      const p4::v1::ReadRequest& req,
      const p4::config::v1::P4Info& p4info) const ABSL_LOCKS_EXCLUDED(lock_);

  absl::Status SendPacketInToPrimary(
      const p4::v1::StreamMessageResponse& response) ABSL_LOCKS_EXCLUDED(lock_);

  absl::Status SendStreamMessageToPrimary(
      const p4::v1::StreamMessageResponse& response) ABSL_LOCKS_EXCLUDED(lock_);

 private:
  SdnControllerManager() : device_id_(0) {}

  // Goes through the current list of active connections, and returns if one of
  // them is currently the primary.
  bool PrimaryConnectionExists(const absl::optional<std::string>& role_name)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Sends an arbitration update to all active connections for a role about the
  // current primary connection.
  void InformConnectionsAboutPrimaryChange(
      const absl::optional<std::string>& role_name)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Sends an arbitration update to a specific connection.
  void SendArbitrationResponse(SdnConnection* connection)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Lock for protecting SdnControllerManager member fields.
  mutable absl::Mutex lock_;

  // Device ID is used to ensure all requests are connecting to the intended
  // place.
  const uint64_t device_id_;

  // We maintain a list of all active connections. The P4 runtime spec requires
  // a number of edge cases based on values existing or not that makes
  // maintaining these connections any other container difficult. However, we
  // don't expect a large number of connections so performance shouldn't be
  // affected. These are non-owning pointers.
  //
  // Requirements for roles:
  //  * Each role can have it's own set of primary & backup connections.
  //  * If no role is specified (NOTE: different than "") the role is assumed to
  //    be 'root', and as such has access to any table in the P4 application.
  //
  // Requirements for election ids:
  //  * The connection with the highest election ID is the primary.
  //  * If no election ID is given (NOTE: different than 0) the connection is
  //    valid, but it cannot ever be primary (i.e. the controller can force a
  //    connection to be a backup).
  std::vector<SdnConnection*> connections_ ABSL_GUARDED_BY(lock_);

  // We maintain a map of the latest role config set for a given role.
  //
  // key:   role_name   (no value indicates the default/root role)
  // value: role config (no value indicates unrestricted access)
  absl::flat_hash_map<absl::optional<std::string>, absl::optional<P4RoleConfig>>
      role_config_by_name_ ABSL_GUARDED_BY(lock_){
          {kP4RuntimeRoleSdnController, {}},
          {absl::nullopt, {}},  // default role
      };

  // We maintain a map of the highest election IDs that have been selected for
  // the primary connection of a role. Once an election ID is set all new
  // primary connections for that role must use an election ID that is >= in
  // value.
  //
  // key:   role_name   (no value indicates the default/root role)
  // value: election ID (no value indicates there has never been a primary
  //                     connection)
  absl::flat_hash_map<absl::optional<std::string>,
                      absl::optional<absl::uint128>>
      election_id_past_by_role_ ABSL_GUARDED_BY(lock_);
};

}  // namespace p4runtime
}  // namespace stratum

#endif  // STRATUM_LIB_P4RUNTIME_SDN_CONTROLLER_MANAGER_H_
