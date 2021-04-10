// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/barefoot/bf_interface.h"

namespace stratum {
namespace barefoot {

class BfInterfaceImpl : public BfInterface {
 public:
  //   explicit TableKey(std::unique_ptr<bfrt::BfRtTableKey> table_key)
  //       : table_key_(std::move(table_key)) {}

  ::absl::Status InitSde() override;
  ::absl::Status SetForwardingPipelineConfig(
      const ::p4::v1::SetForwardingPipelineConfigRequest& req,
      ::p4::v1::SetForwardingPipelineConfigResponse* resp) override;
  ::absl::Status GetForwardingPipelineConfig(
      const ::p4::v1::GetForwardingPipelineConfigRequest& req,
      ::p4::v1::GetForwardingPipelineConfigResponse* resp) override;
  ::absl::Status Write(const ::p4::v1::WriteRequest& req,
                       ::p4::v1::WriteResponse* resp) override;
  ::absl::Status Read(const ::p4::v1::ReadRequest& req,
                      ::p4::v1::ReadResponse* resp) override;
};

::absl::Status BfInterfaceImpl::InitSde() { return absl::OkStatus(); }

::absl::Status BfInterfaceImpl::SetForwardingPipelineConfig(
    const ::p4::v1::SetForwardingPipelineConfigRequest& req,
    ::p4::v1::SetForwardingPipelineConfigResponse* resp) {
  return absl::OkStatus();
}

::absl::Status BfInterfaceImpl::GetForwardingPipelineConfig(
    const ::p4::v1::GetForwardingPipelineConfigRequest& req,
    ::p4::v1::GetForwardingPipelineConfigResponse* resp) {
  return absl::OkStatus();
}

::absl::Status BfInterfaceImpl::Write(const ::p4::v1::WriteRequest& req,
                                      ::p4::v1::WriteResponse* resp) {
  return absl::OkStatus();
}

::absl::Status BfInterfaceImpl::Read(const ::p4::v1::ReadRequest& req,
                                     ::p4::v1::ReadResponse* resp) {
  return absl::OkStatus();
}

}  // namespace barefoot
}  // namespace stratum








namespace {

static ::stratum::barefoot::BfInterface* bf_interface_ = NULL;

// A macro for simplify checking of arguments in the C functions.
#define CHECK_RETURN_IF_FALSE(cond) \
  if (ABSL_PREDICT_TRUE(cond)) {    \
  } else /* NOLINT */               \
    return static_cast<int>(::absl::StatusCode::kInvalidArgument)

// A macro that converts an absl::Status to an int and returns it.
#define RETURN_STATUS(status) return static_cast<int>(status.code())

// TODO(bocon): consider free if response not null
// A macro that converts between binary and C++ representations of
// a protobuf. The C++ objects are used to call the C++ function.
#define RETURN_CPP_API(RequestProto, ResponseProto, Function)                  \
  CHECK_RETURN_IF_FALSE(packed_response == NULL);                              \
  RequestProto request;                                                        \
  CHECK_RETURN_IF_FALSE(request.ParseFromArray(packed_request, request_size)); \
  ResponseProto response;                                                      \
  ::absl::Status status = bf_interface_->Function(request, &response);         \
  response_size = response.ByteSizeLong();                                     \
  packed_response = malloc(response_size);                                     \
  response.SerializeToArray(packed_response, response_size);                   \
  RETURN_STATUS(status)

}  // namespace

int bf_init() {
  // TODO: Allocate bf_interface_
  // TODO: Initialize the SDE
  return 0;
}

int bf_destroy() {
  // TODO: Free bf_interface_
  return 0;
}

int bf_p4_set_pipeline_config(const PackedProtobuf packed_request,
                              size_t request_size,
                              PackedProtobuf& packed_response,
                              size_t& response_size) {
  RETURN_CPP_API(::p4::v1::SetForwardingPipelineConfigRequest,
                 ::p4::v1::SetForwardingPipelineConfigResponse,
                 SetForwardingPipelineConfig);
}

int bf_p4_get_pipeline_config(const PackedProtobuf packed_request,
                              size_t request_size,
                              PackedProtobuf& packed_response,
                              size_t& response_size) {
  RETURN_CPP_API(::p4::v1::GetForwardingPipelineConfigRequest,
                 ::p4::v1::GetForwardingPipelineConfigResponse,
                 GetForwardingPipelineConfig);
}

int bf_p4_write(const PackedProtobuf packed_request, size_t request_size,
                PackedProtobuf& packed_response, size_t& response_size) {
  RETURN_CPP_API(::p4::v1::WriteRequest, ::p4::v1::WriteResponse, Write);
}

int bf_p4_read(const PackedProtobuf packed_request, size_t request_size,
               PackedProtobuf& packed_response, size_t& response_size) {
  RETURN_CPP_API(::p4::v1::ReadRequest, ::p4::v1::ReadResponse, Read);
}
