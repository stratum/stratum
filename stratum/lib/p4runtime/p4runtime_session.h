// Copyright 2020 Google LLC
// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_P4RUNTIME_P4RUNTIME_SESSION_H_
#define STRATUM_LIB_P4RUNTIME_P4RUNTIME_SESSION_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/numeric/int128.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "grpcpp/security/credentials.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/public/proto/p4_role_config.pb.h"

namespace stratum {
namespace p4runtime {
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
// lower 64 bits are the remaining milliseconds. This is compatible with
// election-systems that use the same epoch-based election IDs, and in that
// case, this election ID will be guaranteed to be higher than any previous
// election ID.
inline absl::uint128 TimeBasedElectionId() {
  uint64 msec = absl::ToUnixMillis(absl::Now());
  return absl::MakeUint128(msec / 1000, msec % 1000);
}

// Returns the gRPC ChannelArguments for P4Runtime by setting
// `GRPC_ARG_KEEPALIVE_TIME_MS` (to avoid connection problems) and
// `GRPC_ARG_MAX_METADATA_SIZE` (P4RT returns batch element status in the
// ::grpc::Status, which can require a large metadata size) and
// `SetMaxReceiveMessageSize` (to fetch large P4 pipeline configs).
inline ::grpc::ChannelArguments GrpcChannelArgumentsForP4rt() {
  ::grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 300000 /*5 minutes*/);
  args.SetInt(GRPC_ARG_MAX_METADATA_SIZE, P4GRPCMaxMetadataSize());
  args.SetMaxReceiveMessageSize(P4GRPCMaxMessageReceiveSize());
  return args;
}

// Creates P4Runtime stub with appropriate channel configuration.
std::unique_ptr<::p4::v1::P4Runtime::Stub> CreateP4RuntimeStub(
    const std::string& address,
    const std::shared_ptr<grpc::ChannelCredentials>& credentials);

// A P4Runtime session.
class P4RuntimeSession {
 public:
  // Creates a session with the switch, which lasts until the session object is
  // destructed.
  static ::util::StatusOr<std::unique_ptr<P4RuntimeSession>> Create(
      std::unique_ptr<::p4::v1::P4Runtime::Stub> stub, uint32 device_id,
      absl::uint128 election_id = TimeBasedElectionId(),
      absl::optional<std::string> role_name = absl::nullopt,
      absl::optional<P4RoleConfig> role_config = absl::nullopt);

  // Creates a session with the switch, which lasts until the session object is
  // destructed.
  static ::util::StatusOr<std::unique_ptr<P4RuntimeSession>> Create(
      const std::string& address,
      const std::shared_ptr<grpc::ChannelCredentials>& credentials,
      uint32 device_id, absl::uint128 election_id = TimeBasedElectionId(),
      absl::optional<std::string> role_name = absl::nullopt,
      absl::optional<P4RoleConfig> role_config = absl::nullopt);

  // Connects to the default session on the switch, which has no election_id
  // and which cannot be terminated. This should only be used for testing.
  // The stream_channel and stream_channel_context will be the nullptr.
  static std::unique_ptr<P4RuntimeSession> Default(
      std::unique_ptr<::p4::v1::P4Runtime::Stub> stub, uint32 device_id);

  ~P4RuntimeSession();

  // Disables copy semantics.
  P4RuntimeSession(const P4RuntimeSession&) = delete;
  P4RuntimeSession& operator=(const P4RuntimeSession&) = delete;

  // Allows move semantics.
  P4RuntimeSession(P4RuntimeSession&&) = default;
  P4RuntimeSession& operator=(P4RuntimeSession&&) = default;

  // Returns the id of the node that this session belongs to.
  uint32 DeviceId() const { return device_id_; }
  // Returns the election id that has been used to perform master arbitration.
  ::p4::v1::Uint128 ElectionId() const { return election_id_; }
  // Returns the P4Runtime stub.
  ::p4::v1::P4Runtime::Stub& Stub() { return *stub_; }
  // Reads back stream message response.
  ABSL_MUST_USE_RESULT bool StreamChannelRead(
      ::p4::v1::StreamMessageResponse* response) {
    return stream_channel_->Read(response);
  }
  // Writes stream message request.
  ABSL_MUST_USE_RESULT bool StreamChannelWrite(
      const ::p4::v1::StreamMessageRequest& request) {
    return stream_channel_->Write(request);
  }
  // Sets the forwarding pipeline from the given p4 info and device config.
  ::util::Status SetForwardingPipelineConfig(
      const ::p4::config::v1::P4Info& p4info,
      const std::string& p4_device_config);
  // Gets the current forwarding pipeline from the switch.
  ::util::Status GetForwardingPipelineConfig(::p4::config::v1::P4Info* p4info,
                                             std::string* p4_device_config);
  // Cancels the StreamChannel RPC. It is done in a best-effort fashion.
  void TryCancel() { stream_channel_context_->TryCancel(); }
  // Closes the RPC connection by telling the server it is done writing. Once
  // the server finishes handling all outstanding writes it will close.
  ::util::Status Finish();

 private:
  P4RuntimeSession(uint32 device_id,
                   std::unique_ptr<::p4::v1::P4Runtime::Stub> stub,
                   absl::uint128 election_id,
                   absl::optional<std::string> role_name = absl::nullopt,
                   absl::optional<P4RoleConfig> role_config = absl::nullopt)
      : device_id_(device_id),
        role_name_(role_name),
        role_config_(role_config),
        stub_(std::move(stub)),
        stream_channel_context_(absl::make_unique<grpc::ClientContext>()),
        stream_channel_(stub_->StreamChannel(stream_channel_context_.get())) {
    election_id_.set_high(absl::Uint128High64(election_id));
    election_id_.set_low(absl::Uint128Low64(election_id));
  }

  // The id of the node that this session belongs to.
  uint32 device_id_;
  // The election id that has been used to perform master arbitration.
  ::p4::v1::Uint128 election_id_;
  // The optional role name that has been used to perform master arbitration.
  absl::optional<std::string> role_name_;
  // The optional role config that has been used to perform master arbitration.
  absl::optional<P4RoleConfig> role_config_;
  // The P4Runtime stub of the switch that this session belongs to.
  std::unique_ptr<::p4::v1::P4Runtime::Stub> stub_;
  // This stream channel and context are used to perform master arbitration,
  // but can now also be used for packet IO.
  // Note that declaration order of context and channel is significant, as
  // the context must be initialized before it can be used to create the channel
  // in the contructor.
  std::unique_ptr<grpc::ClientContext> stream_channel_context_;
  std::unique_ptr<grpc::ClientReaderWriterInterface<
      ::p4::v1::StreamMessageRequest, ::p4::v1::StreamMessageResponse>>
      stream_channel_;
};

}  // namespace p4runtime
}  // namespace stratum

#endif  // STRATUM_LIB_P4RUNTIME_P4RUNTIME_SESSION_H_
