// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_SERVER_WRITER_WRAPPER_H_
#define STRATUM_HAL_LIB_COMMON_SERVER_WRITER_WRAPPER_H_

#include "grpcpp/grpcpp.h"
#include "stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {

// Wrapper for ::grpc::ServerWriter based on WriterInterface class.
template <typename T>
class ServerWriterWrapper : public WriterInterface<T> {
 public:
  explicit ServerWriterWrapper(::grpc::ServerWriter<T>* writer)
      : writer_(writer) {}
  bool Write(const T& msg) override {
    VLOG(1) << "ServerWriterWrapper write: " << msg.ShortDebugString();
    if (writer_) return writer_->Write(msg);
    return false;
  }

 private:
  ::grpc::ServerWriter<T>* writer_;  // not owned by the class.
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_SERVER_WRITER_WRAPPER_H_
