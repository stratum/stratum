// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_FILE_SERVICE_H_
#define STRATUM_HAL_LIB_COMMON_FILE_SERVICE_H_

#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "gnoi/file/file.grpc.pb.h"
#include "grpcpp/grpcpp.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/lib/security/auth_policy_checker.h"

namespace stratum {
namespace hal {

// FileService is an implementation of gnoi::file::file gRPC service and
// is in charge of providing all file related APIs: get/put/remove/stat. Clients
// should be able to transfer files as stream of bytes to/from the device using
// these APIs.
class FileService final : public ::gnoi::file::File::Service {
 public:
  // Input parameters:
  // mode: The mode of operation.
  // switch_interface: The pointer to the implementation of SwitchInterface for
  //     all the low-level platform-specific operations.
  // auth_policy_checker: for per RPC authorization policy checks.
  // error_buffer: pointer to an ErrorBuffer for logging all critical errors.
  FileService(OperationMode mode, SwitchInterface* switch_interface,
              AuthPolicyChecker* auth_policy_checker,
              ErrorBuffer* error_buffer);
  ~FileService() override {}

  // Sets up the service in coldboot or warmboot mode.
  ::util::Status Setup(bool warmboot);

  // Tears down the class. Called in both warmboot or coldboot mode.
  ::util::Status Teardown();

  // Please see //openconfig/gnoi/file/file.proto for the
  // documentation of the RPCs.
  ::grpc::Status Get(
      ::grpc::ServerContext* context, const ::gnoi::file::GetRequest* req,
      ::grpc::ServerWriter<::gnoi::file::GetResponse>* writer) override;

  ::grpc::Status Put(::grpc::ServerContext* context,
                     ::grpc::ServerReader<::gnoi::file::PutRequest>* reader,
                     ::gnoi::file::PutResponse* resp) override;

  ::grpc::Status Stat(::grpc::ServerContext* context,
                      const ::gnoi::file::StatRequest* req,
                      ::gnoi::file::StatResponse* resp) override;

  ::grpc::Status Remove(::grpc::ServerContext* context,
                        const ::gnoi::file::RemoveRequest* req,
                        ::gnoi::file::RemoveResponse* resp) override;

  // FileService is neither copyable nor movable.
  FileService(const FileService&) = delete;
  FileService& operator=(const FileService&) = delete;

 private:
  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Stratum stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Stratum stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Stratum stack runs in simulation mode.
  // Note that this variable is set upon initialization and is never changed
  // afterwards.
  const OperationMode mode_;

  // Pointer to SwitchInterface implementation, which encapsulates all the
  // switch capabilities. Not owned by this class.
  SwitchInterface* switch_interface_;

  // Pointer to AuthPolicyChecker. Not owned by this class.
  AuthPolicyChecker* auth_policy_checker_;

  // Pointer to ErrorBuffer to save any critical errors we encounter. Not owned
  // by this class.
  ErrorBuffer* error_buffer_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_FILE_SERVICE_H_
