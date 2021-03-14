// Copyright 2020 Google LLC
// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_TOOLS_BENCHMARK_P4RUNTIME_SESSION_H_
#define STRATUM_TOOLS_BENCHMARK_P4RUNTIME_SESSION_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/numeric/int128.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "grpcpp/security/credentials.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace tools {
namespace benchmark {
// The maximum metadata size that a P4Runtime client should accept.  This is
// necessary, because the P4Runtime protocol returns individual errors to
// requests in a batch all wrapped in a single status, which counts towards the
// metadata size limit.  For large batches, this easily exceeds the default of
// 8KB.
constexpr int P4GRPCMaxMetadataSize() {
  // 4MB.  Assuming 100 bytes per error, this will support batches of around
  // 40000 entries without exceeding the maximum metadata size.
  return 4 * 1024 * 1024;
}

constexpr int P4GRPCMaxMessageReceiveSize() {
  // 256MB. Tofino pipelines can be quite large. This will support reading most
  // pipelines.
  return 256 * 1024 * 1024;
}

// Generates an election id that is monotonically increasing with time.
// Specifically, the upper 64 bits are the unix timestamp in seconds, and the
// lower 64 bits are 0. This is compatible with election-systems that use the
// same epoch-based election IDs, and in that case, this election ID will be
// guaranteed to be higher than any previous election ID.
inline absl::uint128 TimeBasedElectionId() {
  return absl::MakeUint128(absl::ToUnixSeconds(absl::Now()), 0);
}

// A P4Runtime session.
class P4RuntimeSession {
 public:
  // Creates a session with the switch, which lasts until the session object is
  // destructed.
  static ::util::StatusOr<std::unique_ptr<P4RuntimeSession>> Create(
      std::unique_ptr<p4::v1::P4Runtime::Stub> stub, uint32_t device_id,
      absl::uint128 election_id = TimeBasedElectionId());

  // Creates a session with the switch, which lasts until the session object is
  // destructed.
  static ::util::StatusOr<std::unique_ptr<P4RuntimeSession>> Create(
      const std::string& address,
      const std::shared_ptr<grpc::ChannelCredentials>& credentials,
      uint32_t device_id, absl::uint128 election_id = TimeBasedElectionId());

  // Connects to the default session on the switch, which has no election_id
  // and which cannot be terminated. This should only be used for testing.
  // The stream_channel and stream_channel_context will be the nullptr.
  static std::unique_ptr<P4RuntimeSession> Default(
      std::unique_ptr<p4::v1::P4Runtime::Stub> stub, uint32_t device_id);

  // Disable copy semantics.
  P4RuntimeSession(const P4RuntimeSession&) = delete;
  P4RuntimeSession& operator=(const P4RuntimeSession&) = delete;

  // Allow move semantics.
  P4RuntimeSession(P4RuntimeSession&&) = default;
  P4RuntimeSession& operator=(P4RuntimeSession&&) = default;

  // Return the id of the node that this session belongs to.
  uint32_t DeviceId() const { return device_id_; }
  // Return the election id that has been used to perform master arbitration.
  p4::v1::Uint128 ElectionId() const { return election_id_; }
  // Return the P4Runtime stub.
  p4::v1::P4Runtime::Stub& Stub() { return *stub_; }

 private:
  P4RuntimeSession(uint32_t device_id,
                   std::unique_ptr<p4::v1::P4Runtime::Stub> stub,
                   absl::uint128 election_id)
      : device_id_(device_id),
        stub_(std::move(stub)),
        stream_channel_context_(absl::make_unique<grpc::ClientContext>()),
        stream_channel_(stub_->StreamChannel(stream_channel_context_.get())) {
    election_id_.set_high(absl::Uint128High64(election_id));
    election_id_.set_low(absl::Uint128Low64(election_id));
  }

  // The id of the node that this session belongs to.
  uint32_t device_id_;
  // The election id that has been used to perform master arbitration.
  p4::v1::Uint128 election_id_;
  // The P4Runtime stub of the switch that this session belongs to.
  std::unique_ptr<p4::v1::P4Runtime::Stub> stub_;

  // This stream channel and context are used to perform master arbitration,
  // but can now also be used for packet IO.
  std::unique_ptr<grpc::ClientContext> stream_channel_context_;
  std::unique_ptr<grpc::ClientReaderWriter<p4::v1::StreamMessageRequest,
                                           p4::v1::StreamMessageResponse>>
      stream_channel_;
};

// Create P4Runtime stub.
std::unique_ptr<p4::v1::P4Runtime::Stub> CreateP4RuntimeStub(
    const std::string& address,
    const std::shared_ptr<grpc::ChannelCredentials>& credentials);

// Create secure TLS gRPC channel credentials.
inline ::util::StatusOr<std::shared_ptr<grpc_impl::ChannelCredentials>>
CreateTlsChannelCredentials(const std::string& pem_root_certs,
                            const std::string& client_cert,
                            const std::string& client_private_key) {
  auto key_materials_config =
      std::make_shared<::grpc_impl::experimental::TlsKeyMaterialsConfig>();
  key_materials_config->set_pem_root_certs(pem_root_certs);
  ::grpc_impl::experimental::TlsKeyMaterialsConfig::PemKeyCertPair
      pem_key_cert_pair;
  pem_key_cert_pair.cert_chain = client_cert;
  pem_key_cert_pair.private_key = client_private_key;
  key_materials_config->add_pem_key_cert_pair(pem_key_cert_pair);
  auto cred_opts = ::grpc_impl::experimental::TlsCredentialsOptions(
      GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE, GRPC_TLS_SERVER_VERIFICATION,
      key_materials_config, nullptr, nullptr);

  return grpc::experimental::TlsCredentials(cred_opts);
}

// Free-standing functions that operate on a P4RuntimeSession.

// Sends a read request.
::util::StatusOr<p4::v1::ReadResponse> SendReadRequest(
    P4RuntimeSession* session, const p4::v1::ReadRequest& read_request);

// Sends a write request.
::util::Status SendWriteRequest(P4RuntimeSession* session,
                                const p4::v1::WriteRequest& write_request);

// Reads table entries.
::util::StatusOr<std::vector<p4::v1::TableEntry>> ReadTableEntries(
    P4RuntimeSession* session);
::util::StatusOr<std::vector<p4::v1::TableEntry>> ReadTableEntries(
    P4RuntimeSession* session, bool include_counter_data,
    bool include_meter_config);

// Reads indirect counter entries.
// TODO(max): passing in raw the counter id is ugly.
::util::StatusOr<std::vector<p4::v1::CounterEntry>> ReadCounterEntries(
    P4RuntimeSession* session, int counter_id);

// Removes table entries on the switch.
::util::Status RemoveTableEntries(P4RuntimeSession* session,
                                  absl::Span<const p4::v1::TableEntry> entries);

// Clears the table entries
::util::Status ClearTableEntries(P4RuntimeSession* session);

// Installs the given table entry on the switch.
::util::Status InstallTableEntry(P4RuntimeSession* session,
                                 const p4::v1::TableEntry& entry);

// Installs the given table entries on the switch.
::util::Status InstallTableEntries(
    P4RuntimeSession* session, absl::Span<const p4::v1::TableEntry> entries);

// Writes the given counter entries on the switch.
::util::Status ModifyIndirectCounterEntries(
    P4RuntimeSession* session, absl::Span<const p4::v1::CounterEntry> entries);

// Sets the forwarding pipeline from the given p4 info.
::util::Status SetForwardingPipelineConfig(P4RuntimeSession* session,
                                           const p4::config::v1::P4Info& p4info,
                                           const std::string& p4_device_config);

// Gets the current forwarding pipeline from the switch.
::util::Status GetForwardingPipelineConfig(P4RuntimeSession* session,
                                           p4::config::v1::P4Info* p4info,
                                           std::string* p4_device_config);

}  // namespace benchmark
}  // namespace tools
}  // namespace stratum

#endif  // STRATUM_TOOLS_BENCHMARK_P4RUNTIME_SESSION_H_
