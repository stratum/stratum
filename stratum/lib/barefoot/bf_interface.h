// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_BAREFOOT_BF_INTERFACE_H_
#define STRATUM_LIB_BAREFOOT_BF_INTERFACE_H_

// Define BfInterface C++ class.

#ifdef __cplusplus

#include <vector>

#include "absl/status/status.h"
#include "p4/v1/p4runtime.pb.h"

namespace stratum {
namespace barefoot {

// TODO The "BfSdeInterface" class in HAL implements a shim layer around the
// Barefoot
class BfInterface {
 public:
  virtual ::absl::Status InitSde() = 0;

  // Pushes the P4-based forwarding pipeline configuration of one or more
  // switching nodes.
  virtual ::absl::Status SetForwardingPipelineConfig(
      const ::p4::v1::SetForwardingPipelineConfigRequest& req,
      ::p4::v1::SetForwardingPipelineConfigResponse* resp) = 0;

  // Gets the P4-based forwarding pipeline configuration of one or more
  // switching nodes previously pushed to the switch.
  virtual ::absl::Status GetForwardingPipelineConfig(
      const ::p4::v1::GetForwardingPipelineConfigRequest& req,
      ::p4::v1::GetForwardingPipelineConfigResponse* resp) = 0;

  // Writes one or more forwarding entries on the target as part of P4Runtime
  // API. Entries include tables entries, action profile members/groups, meter
  // entries, and counter entries.
  virtual ::absl::Status Write(const ::p4::v1::WriteRequest& req,
                               ::p4::v1::WriteResponse* resp) = 0;

  // Reads the forwarding entries that have been previously written on the
  // target as part of P4Runtime API.
  virtual ::absl::Status Read(const ::p4::v1::ReadRequest& req,
                              ::p4::v1::ReadResponse* resp) = 0;

  // Bidirectional channel between controller and the switch for packet I/O,
  // master arbitration and stream errors.
  //   virtual ::absl::Status StreamChannel(
  //       ::grpc::ServerContext* context,
  //       ServerStreamChannelReaderWriter* stream) override;
};

}  // namespace barefoot
}  // namespace stratum

#endif  //  __cplusplus

// Define C functions to access BfInterface C++ class.
#ifdef __cplusplus
extern "C" {
#endif

#include <cstddef>

typedef void* PackedProtobuf;

int bf_init();

int bf_destroy();

int bf_p4_set_pipeline_config(const PackedProtobuf packed_request,
                              size_t request_size,
                              PackedProtobuf& packed_response,
                              size_t& response_size);

int bf_p4_get_pipeline_config(const PackedProtobuf packed_request,
                              size_t request_size,
                              PackedProtobuf& packed_response,
                              size_t& response_size);

int bf_p4_write(const PackedProtobuf packed_write_request, size_t request_size,
                PackedProtobuf& packed_read_response, size_t& response_size);

int bf_p4_read(const PackedProtobuf packed_read_request, size_t request_size,
               PackedProtobuf& packed_read_response, size_t& response_size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STRATUM_LIB_BAREFOOT_BF_INTERFACE_H_
