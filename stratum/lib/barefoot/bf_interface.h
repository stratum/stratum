// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_BAREFOOT_BF_INTERFACE_H_
#define STRATUM_LIB_BAREFOOT_BF_INTERFACE_H_

// Define C functions to access BfInterface C++ class.
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

// Type for the binary representation of a Protobuf message.
typedef void* PackedProtobuf;

int bf_p4_init(const char* bf_sde_install, const char* bf_switchd_cfg,
               bool bf_switchd_background);

int bf_p4_destroy();

int bf_p4_set_pipeline_config(const PackedProtobuf packed_request,
                              size_t request_size,
                              PackedProtobuf* packed_response,
                              size_t* response_size);

int bf_p4_get_pipeline_config(const PackedProtobuf packed_request,
                              size_t request_size,
                              PackedProtobuf* packed_response,
                              size_t* response_size);

int bf_p4_write(const PackedProtobuf packed_request, size_t request_size,
                PackedProtobuf* packed_response, size_t* response_size);

int bf_p4_read(const PackedProtobuf packed_request, size_t request_size,
               PackedProtobuf* packed_response, size_t* response_size);

#ifdef __cplusplus
}  // extern "C"
#endif

// Define BfInterface C++ class.

#ifdef __cplusplus

#include <vector>

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.pb.h"

namespace stratum {
namespace barefoot {

// TODO(bocon): The "BfSdeInterface" class in HAL implements a shim layer
// around the Barefoot
class BfInterface {
 public:
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

  virtual ::absl::Status InitSde(absl::string_view bf_sde_install,
                                 absl::string_view bf_switchd_cfg,
                                 bool bf_switchd_background) = 0;

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static BfInterface* CreateSingleton() LOCKS_EXCLUDED(init_lock_);

  // Return the singleton instance to be used in the SDE callbacks.
  static BfInterface* GetSingleton() LOCKS_EXCLUDED(init_lock_);

 protected:
  // RW mutex lock for protecting the singleton instance initialization and
  // reading it back from other threads. Unlike other singleton classes, we
  // use RW lock as we need the pointer to class to be returned.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static BfInterface* singleton_ GUARDED_BY(init_lock_);
};

}  // namespace barefoot
}  // namespace stratum

#endif  //  __cplusplus

#endif  // STRATUM_LIB_BAREFOOT_BF_INTERFACE_H_
