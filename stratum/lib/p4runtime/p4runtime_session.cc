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
using ::p4::v1::PacketIn;
using ::p4::v1::PacketOut;
using ::p4::v1::ReadRequest;
using ::p4::v1::ReadResponse;
using ::p4::v1::SetForwardingPipelineConfigRequest;
using ::p4::v1::SetForwardingPipelineConfigResponse;
using ::p4::v1::StreamMessageResponse;
using ::p4::v1::TableEntry;
using ::p4::v1::Update;
using ::p4::v1::WriteRequest;
using ::p4::v1::WriteResponse;

namespace {
// Helper functions to convert between ::util::Status and ::grpc::Status.
::grpc::Status StatusToGrpcStatus(const ::util::Status& status) {
  return ::grpc::Status(static_cast<grpc::StatusCode>(status.error_code()),
                        std::string(status.error_message()));
}

::util::Status GrpcStatusToStatus(const ::grpc::Status& status) {
  return ::util::Status(static_cast<::util::error::Code>(status.error_code()),
                        status.error_message());
}
}  // namespace

std::unique_ptr<P4Runtime::Stub> CreateP4RuntimeStub(
    const std::string& address,
    const std::shared_ptr<grpc::ChannelCredentials>& credentials) {
  return P4Runtime::NewStub(grpc::CreateCustomChannel(
      address, credentials, GrpcChannelArgumentsForP4rt()));
}

::util::StatusOr<std::unique_ptr<P4RuntimeSession>> P4RuntimeSession::Create(
    std::unique_ptr<P4Runtime::Stub> stub, uint32 device_id,
    absl::uint128 election_id, absl::optional<std::string> role_name,
    absl::optional<P4RoleConfig> role_config) {
  RET_CHECK(role_name.has_value() || !role_config.has_value())
      << "Cannot set a role config for the default role.";

  // Open streaming channel.
  // Using `new` to access a private constructor.
  std::unique_ptr<P4RuntimeSession> session =
      absl::WrapUnique(new P4RuntimeSession(
          device_id, std::move(stub), election_id, role_name, role_config));

  // Send arbitration request.
  ::p4::v1::StreamMessageRequest request;
  auto arbitration = request.mutable_arbitration();
  arbitration->set_device_id(device_id);
  *arbitration->mutable_election_id() = session->election_id_;
  if (role_name.has_value()) {
    arbitration->mutable_role()->set_name(role_name.value());
    if (role_config.has_value()) {
      arbitration->mutable_role()->mutable_config()->PackFrom(
          role_config.value());
    }
  }
  if (!session->stream_channel_->Write(request)) {
    return MAKE_ERROR(ERR_UNAVAILABLE)
           << "Unable to initiate P4RT connection to device ID " << device_id
           << "; gRPC stream channel closed.";
  }

  // Wait for arbitration response.
  ::p4::v1::StreamMessageResponse response;
  if (!session->stream_channel_->Read(&response)) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "P4RT stream closed while awaiting arbitration response: "
           << GrpcStatusToStatus(session->stream_channel_->Finish());
  }
  if (response.update_case() != ::p4::v1::StreamMessageResponse::kArbitration) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "No arbitration update received but received the update of "
           << response.update_case() << ": " << response.ShortDebugString();
  }
  if (response.arbitration().device_id() != session->device_id_) {
    return MAKE_ERROR(ERR_INTERNAL) << "Received device id doesn't match: "
                                    << response.ShortDebugString();
  }
  if (response.arbitration().election_id().high() !=
      session->election_id_.high()) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Highest 64 bits of received election id doesn't match: "
           << response.ShortDebugString();
  }
  if (response.arbitration().election_id().low() !=
      session->election_id_.low()) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Lowest 64 bits of received election id doesn't match: "
           << response.ShortDebugString();
  }
  if (role_name.has_value()) {
    if (response.arbitration().role().name() != session->role_name_) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Role name of received role doesn't match: "
             << response.ShortDebugString();
    }
    if (role_config.has_value()) {
      P4RoleConfig received_role_config;
      if (!response.arbitration().role().config().UnpackTo(
              &received_role_config)) {
        return MAKE_ERROR(ERR_INTERNAL)
               << "Role config of received role has invalid format: "
               << response.ShortDebugString();
      }
      if (!ProtoEqual(received_role_config, session->role_config_.value())) {
        return MAKE_ERROR(ERR_INTERNAL)
               << "Role config of received role doesn't match: "
               << response.ShortDebugString();
      }
    }
  }

  // When object returned doesn't have the same type as the function's return
  // type (i.e. unique_ptr vs StatusOr in this case), certain old compilers
  // won't implicitly wrap the return expressions in std::move(). Then, the case
  // here will trigger the copy of the unique_ptr, which is invalid. Thus, we
  // need to explicitly std::move the returned object here.
  // See: https://abseil.io/tips/11 and https://abseil.io/tips/77
  return std::move(session);
}

// TODO(max): Is this a good idea? Why do we need this?
P4RuntimeSession::~P4RuntimeSession() {
  if (stream_channel_) {
    TryCancel();
  }
}

// Creates a session with the switch, which lasts until the session object is
// destructed.
::util::StatusOr<std::unique_ptr<P4RuntimeSession>> P4RuntimeSession::Create(
    const std::string& address,
    const std::shared_ptr<grpc::ChannelCredentials>& credentials,
    uint32 device_id, absl::uint128 election_id,
    absl::optional<std::string> role_name,
    absl::optional<P4RoleConfig> role_config) {
  return Create(CreateP4RuntimeStub(address, credentials), device_id,
                election_id, role_name, role_config);
}

// Create the default session with the switch.
std::unique_ptr<P4RuntimeSession> P4RuntimeSession::Default(
    std::unique_ptr<P4Runtime::Stub> stub, uint32 device_id) {
  // Using `new` to access a private constructor.
  return absl::WrapUnique(
      new P4RuntimeSession(device_id, std::move(stub), device_id));
}

::util::Status P4RuntimeSession::Finish() {
  stream_channel_->WritesDone();

  // WritesDone() or TryCancel() can close the stream with a CANCELLED status.
  // Because this case is expected we treat CANCELED as OKAY.
  ::grpc::Status finish = stream_channel_->Finish();
  if (finish.error_code() == ::grpc::StatusCode::CANCELLED) {
    return ::util::OkStatus();
  }
  return GrpcStatusToStatus(finish);
}

::util::Status P4RuntimeSession::SetForwardingPipelineConfig(
    const P4Info& p4info, const std::string& p4_device_config) {
  SetForwardingPipelineConfigRequest request;
  request.set_device_id(device_id_);
  *request.mutable_election_id() = election_id_;
  role_name_.has_value() ? request.set_role(role_name_.value())
                         : request.clear_role();
  request.set_action(SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  *request.mutable_config()->mutable_p4info() = p4info;
  *request.mutable_config()->mutable_p4_device_config() = p4_device_config;

  // Empty message; intentionally discarded.
  SetForwardingPipelineConfigResponse response;
  grpc::ClientContext context;
  return GrpcStatusToStatus(
      stub_->SetForwardingPipelineConfig(&context, request, &response));
}

::util::Status P4RuntimeSession::GetForwardingPipelineConfig(
    P4Info* p4info, std::string* p4_device_config) {
  GetForwardingPipelineConfigRequest request;
  request.set_device_id(device_id_);
  request.set_response_type(GetForwardingPipelineConfigRequest::ALL);

  GetForwardingPipelineConfigResponse response;
  grpc::ClientContext context;
  RETURN_IF_ERROR(GrpcStatusToStatus(
      stub_->GetForwardingPipelineConfig(&context, request, &response)));

  *p4info = response.config().p4info();
  *p4_device_config = response.config().p4_device_config();

  return ::util::OkStatus();
}

}  // namespace p4runtime
}  // namespace stratum
