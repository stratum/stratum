// Copyright 2020 Google LLC
// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/p4runtime/p4runtime_session.h"

#include <string>

#include "grpcpp/channel.h"
#include "grpcpp/create_channel.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace p4runtime {

using ::p4::config::v1::P4Info;
using ::p4::v1::CounterEntry;
using ::p4::v1::GetForwardingPipelineConfigRequest;
using ::p4::v1::GetForwardingPipelineConfigResponse;
using ::p4::v1::P4Runtime;
using ::p4::v1::ReadRequest;
using ::p4::v1::ReadResponse;
using ::p4::v1::SetForwardingPipelineConfigRequest;
using ::p4::v1::SetForwardingPipelineConfigResponse;
using ::p4::v1::TableEntry;
using ::p4::v1::ActionProfileMember;
using ::p4::v1::ActionProfileGroup;
using ::p4::v1::DigestEntry;
using ::p4::v1::Update;
using ::p4::v1::WriteRequest;
using ::p4::v1::WriteResponse;

// Create P4Runtime Stub.
std::unique_ptr<P4Runtime::Stub> CreateP4RuntimeStub(
    const std::string& address,
    const std::shared_ptr<grpc::ChannelCredentials>& credentials) {
  grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_MAX_METADATA_SIZE, P4GRPCMaxMetadataSize());
  args.SetMaxReceiveMessageSize(P4GRPCMaxMessageReceiveSize());
  return P4Runtime::NewStub(
      grpc::CreateCustomChannel(address, credentials, args));
}

// Creates a session with the switch, which lasts until the session object is
// destructed.
::util::StatusOr<std::unique_ptr<P4RuntimeSession>> P4RuntimeSession::Create(
    std::unique_ptr<P4Runtime::Stub> stub, uint32_t device_id,
    absl::uint128 election_id) {
  // Open streaming channel.
  // Using `new` to access a private constructor.
  std::unique_ptr<P4RuntimeSession> session = absl::WrapUnique(
      new P4RuntimeSession(device_id, std::move(stub), election_id));

  // Send arbitration request.
  p4::v1::StreamMessageRequest request;
  auto arbitration = request.mutable_arbitration();
  arbitration->set_device_id(device_id);
  *arbitration->mutable_election_id() = session->election_id_;
  if (!session->stream_channel_->Write(request)) {
    RETURN_ERROR(ERR_UNAVAILABLE)
        << "Unable to initiate P4RT connection to device ID " << device_id
        << "; gRPC stream channel closed.";
  }

  // Wait for arbitration response.
  p4::v1::StreamMessageResponse response;
  if (!session->stream_channel_->Read(&response)) {
    RETURN_ERROR(ERR_INTERNAL)
        << "No arbitration response received because: "
        << GrpcStatusToStatus(session->stream_channel_->Finish())
        << " with response: " << response.ShortDebugString();
  }
  if (response.update_case() != p4::v1::StreamMessageResponse::kArbitration) {
    RETURN_ERROR(ERR_INTERNAL)
        << "No arbitration update received but received the update of "
        << response.update_case() << ": " << response.ShortDebugString();
  }
  if (response.arbitration().device_id() != session->device_id_) {
    RETURN_ERROR(ERR_INTERNAL)
        << "Received device id doesn't match: " << response.ShortDebugString();
  }
  if (response.arbitration().election_id().high() !=
      session->election_id_.high()) {
    RETURN_ERROR(ERR_INTERNAL)
        << "Highest 64 bits of received election id doesn't match: "
        << response.ShortDebugString();
  }
  if (response.arbitration().election_id().low() !=
      session->election_id_.low()) {
    RETURN_ERROR(ERR_INTERNAL)
        << "Lowest 64 bits of received election id doesn't match: "
        << response.ShortDebugString();
  }

  // Move is needed to make the older compiler happy.
  // See: go/totw/labs/should-i-return-std-move.
  return std::move(session);
}

// Creates a session with the switch, which lasts until the session object is
// destructed.
::util::StatusOr<std::unique_ptr<P4RuntimeSession>> P4RuntimeSession::Create(
    const std::string& address,
    const std::shared_ptr<grpc::ChannelCredentials>& credentials,
    uint32_t device_id, absl::uint128 election_id) {
  return Create(CreateP4RuntimeStub(address, credentials), device_id,
                election_id);
}

// Create the default session with the switch.
std::unique_ptr<P4RuntimeSession> P4RuntimeSession::Default(
    std::unique_ptr<P4Runtime::Stub> stub, uint32_t device_id) {
  // Using `new` to access a private constructor.
  return absl::WrapUnique(
      new P4RuntimeSession(device_id, std::move(stub), device_id));
}

::util::StatusOr<ReadResponse> SendReadRequest(
    P4RuntimeSession* session, const ReadRequest& read_request) {
  grpc::ClientContext context;
  auto reader = session->Stub().Read(&context, read_request);

  ReadResponse response;
  ReadResponse partial_response;
  while (reader->Read(&partial_response)) {
    response.MergeFrom(partial_response);
  }

  grpc::Status reader_status = reader->Finish();
  if (!reader_status.ok()) {
    return GrpcStatusToStatus(reader_status);
  }

  return std::move(response);
}

::util::Status SendWriteRequest(P4RuntimeSession* session,
                                const WriteRequest& write_request) {
  grpc::ClientContext context;
  // Empty message; intentionally discarded.
  WriteResponse response;

  ::grpc::Status status =
      session->Stub().Write(&context, write_request, &response);
  // TODO(max): pack this into the ::util:Status or return a vector?
  if (!status.ok()) {
    LOG(ERROR) << hal::P4RuntimeGrpcStatusToString(status);
  }

  return GrpcStatusToStatus(status);
}

::util::StatusOr<std::vector<TableEntry>> ReadTableEntries(
    P4RuntimeSession* session) {
  return ReadTableEntries(session, false, false);
}

::util::StatusOr<std::vector<TableEntry>> ReadTableEntries(
    P4RuntimeSession* session, bool include_counter_data,
    bool include_meter_config) {
  ReadRequest read_request;
  read_request.set_device_id(session->DeviceId());
  read_request.add_entities()->mutable_table_entry();
  if (include_counter_data) {
    read_request.mutable_entities(0)
        ->mutable_table_entry()
        ->mutable_counter_data();
  }
  if (include_meter_config) {
    read_request.mutable_entities(0)
        ->mutable_table_entry()
        ->mutable_meter_config();
  }
  ASSIGN_OR_RETURN(ReadResponse read_response,
                   SendReadRequest(session, read_request));

  std::vector<TableEntry> table_entries;
  table_entries.reserve(read_response.entities().size());
  for (const auto& entity : read_response.entities()) {
    if (!entity.has_table_entry())
      RETURN_ERROR(ERR_INTERNAL)
          << "Entity in the read response has no table entry: "
          << entity.DebugString();
    if (include_counter_data && !entity.table_entry().has_counter_data()) {
      RETURN_ERROR(ERR_INTERNAL)
          << "TableEntry in the read response has no counter data: "
          << entity.table_entry().DebugString();
    }
    if (include_meter_config && !entity.table_entry().has_meter_config()) {
      RETURN_ERROR(ERR_INTERNAL)
          << "TableEntry in the read response has no meter config: "
          << entity.table_entry().DebugString();
    }
    table_entries.push_back(std::move(entity.table_entry()));
  }
  return std::move(table_entries);
}

::util::StatusOr<std::vector<CounterEntry>> ReadCounterEntries(
    P4RuntimeSession* session, int counter_id) {
  ReadRequest read_request;
  read_request.set_device_id(session->DeviceId());
  read_request.add_entities()->mutable_counter_entry()->set_counter_id(
      counter_id);
  ASSIGN_OR_RETURN(ReadResponse read_response,
                   SendReadRequest(session, read_request));

  std::vector<CounterEntry> counter_entries;
  counter_entries.reserve(read_response.entities().size());
  for (const auto& entity : read_response.entities()) {
    if (!entity.has_counter_entry())
      RETURN_ERROR(ERR_INTERNAL)
          << "Entity in the read response has no counter entry: "
          << entity.DebugString();
    counter_entries.push_back(std::move(entity.counter_entry()));
  }
  return std::move(counter_entries);
}

::util::Status ClearTableEntries(P4RuntimeSession* session) {
  ASSIGN_OR_RETURN(auto table_entries, ReadTableEntries(session));
  // Early return if there is nothing to clear.
  if (table_entries.empty()) return ::util::OkStatus();
  return RemoveTableEntries(session, table_entries);
}

::util::Status RemoveTableEntries(P4RuntimeSession* session,
                                  absl::Span<const TableEntry> entries) {
  WriteRequest clear_request;
  clear_request.set_device_id(session->DeviceId());
  *clear_request.mutable_election_id() = session->ElectionId();

  for (const auto& table_entry : entries) {
    Update* update = clear_request.add_updates();
    update->set_type(Update::DELETE);
    *update->mutable_entity()->mutable_table_entry() = table_entry;
  }
  return SendWriteRequest(session, clear_request);
}

::util::Status InstallTableEntry(P4RuntimeSession* session,
                                 const TableEntry& entry) {
  return InstallTableEntries(session, absl::MakeConstSpan(&entry, 1));
}

::util::Status InstallTableEntries(P4RuntimeSession* session,
                                   absl::Span<const TableEntry> entries) {
  WriteRequest batch_write_request;
  batch_write_request.set_device_id(session->DeviceId());
  *batch_write_request.mutable_election_id() = session->ElectionId();

  for (const auto& entry : entries) {
    Update* update = batch_write_request.add_updates();
    update->set_type(Update::INSERT);
    *update->mutable_entity()->mutable_table_entry() = entry;
  }
  return SendWriteRequest(session, batch_write_request);
}

::util::Status ModifyTableEntries(P4RuntimeSession* session,
                                  absl::Span<const TableEntry> entries) {
  WriteRequest batch_write_request;
  batch_write_request.set_device_id(session->DeviceId());
  *batch_write_request.mutable_election_id() = session->ElectionId();

  for (const auto& entry : entries) {
    Update* update = batch_write_request.add_updates();
    update->set_type(Update::MODIFY);
    *update->mutable_entity()->mutable_table_entry() = entry;
  }
  return SendWriteRequest(session, batch_write_request);
}

::util::Status ModifyIndirectCounterEntries(
    P4RuntimeSession* session, absl::Span<const CounterEntry> entries) {
  WriteRequest batch_write_request;
  batch_write_request.set_device_id(session->DeviceId());
  *batch_write_request.mutable_election_id() = session->ElectionId();

  for (const auto& entry : entries) {
    Update* update = batch_write_request.add_updates();
    update->set_type(Update::MODIFY);
    *update->mutable_entity()->mutable_counter_entry() = entry;
  }
  return SendWriteRequest(session, batch_write_request);
}

::util::Status InstallActionProfileMemberEntry(
  P4RuntimeSession* session, const ActionProfileMember entry ){
  return InstallActionProfileMemberEntries(session, absl::MakeConstSpan(&entry, 1));
}

::util::Status InstallActionProfileMemberEntries(
  P4RuntimeSession* session, absl::Span<const ActionProfileMember> entries ){

  WriteRequest batch_write_request;
  batch_write_request.set_device_id(session->DeviceId());
  *batch_write_request.mutable_election_id() = session->ElectionId();

  for (const auto& entry : entries) {
    Update* update = batch_write_request.add_updates();
    update->set_type(Update::INSERT);
    *update->mutable_entity()->mutable_action_profile_member() = entry;
  }

  return SendWriteRequest(session, batch_write_request);
}

::util::Status InstallActionProfileGroupEntry(
  P4RuntimeSession* session, const ActionProfileGroup entry ){
  return InstallActionProfileGroupEntries(session, absl::MakeConstSpan(&entry, 1));
}

::util::Status InstallActionProfileGroupEntries(
  P4RuntimeSession* session, absl::Span<const ActionProfileGroup> entries ){

  WriteRequest batch_write_request;
  batch_write_request.set_device_id(session->DeviceId());
  *batch_write_request.mutable_election_id() = session->ElectionId();

  for (const auto& entry : entries) {
    Update* update = batch_write_request.add_updates();
    update->set_type(Update::INSERT);
    *update->mutable_entity()->mutable_action_profile_group() = entry;
  }
  return SendWriteRequest(session, batch_write_request);
}

::util::Status InstallDigestEntry(
  P4RuntimeSession* session, const DigestEntry entry ){
  return InstallDigestEntries(session, absl::MakeConstSpan(&entry, 1));
}

::util::Status InstallDigestEntries(
  P4RuntimeSession* session, absl::Span<const DigestEntry> entries ){

  WriteRequest batch_write_request;
  batch_write_request.set_device_id(session->DeviceId());
  *batch_write_request.mutable_election_id() = session->ElectionId();

  for (const auto& entry : entries) {
    Update* update = batch_write_request.add_updates();
    // Update_Type_MODIFY
    update->set_type(Update::INSERT);
    *update->mutable_entity()->mutable_digest_entry() = entry;
  }
  return SendWriteRequest(session, batch_write_request);
}


::util::Status SetForwardingPipelineConfig(
    P4RuntimeSession* session, const P4Info& p4info,
    const std::string& p4_device_config) {
  SetForwardingPipelineConfigRequest request;
  request.set_device_id(session->DeviceId());
  *request.mutable_election_id() = session->ElectionId();
  request.set_action(SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  *request.mutable_config()->mutable_p4info() = p4info;
  *request.mutable_config()->mutable_p4_device_config() = p4_device_config;

  // Empty message; intentionally discarded.
  SetForwardingPipelineConfigResponse response;
  grpc::ClientContext context;
  return GrpcStatusToStatus(session->Stub().SetForwardingPipelineConfig(
      &context, request, &response));
}

::util::Status GetForwardingPipelineConfig(P4RuntimeSession* session,
                                           P4Info* p4info,
                                           std::string* p4_device_config) {
  GetForwardingPipelineConfigRequest request;
  request.set_device_id(session->DeviceId());
  request.set_response_type(GetForwardingPipelineConfigRequest::ALL);

  GetForwardingPipelineConfigResponse response;
  grpc::ClientContext context;
  RETURN_IF_ERROR(
      GrpcStatusToStatus(session->Stub().GetForwardingPipelineConfig(
          &context, request, &response)));

  *p4info = response.config().p4info();
  *p4_device_config = response.config().p4_device_config();

  return ::util::OkStatus();
}

}  // namespace p4runtime
}  // namespace stratum
