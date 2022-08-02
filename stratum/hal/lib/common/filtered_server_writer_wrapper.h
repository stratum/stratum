// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_FILTERED_SERVER_WRITER_WRAPPER_H_
#define STRATUM_HAL_LIB_COMMON_FILTERED_SERVER_WRITER_WRAPPER_H_

#include "grpcpp/grpcpp.h"
#include "stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {

// .... Wrapper for ::grpc::ServerWriter based on WriterInterface class.
template <typename T>
class FilteredServerWriterWrapper : public WriterInterface<T> {
 public:
  explicit FilteredServerWriterWrapper(::grpc::ServerWriter<T>* writer,
                                       std::function<T(const T&)> f)
      : writer_(writer), g_(f) {}
  bool Write(const T& msg) override {
    if (!writer_) return false;
    auto t = g_(msg);
    return writer_->Write(t);
  }

 private:
  ::grpc::ServerWriter<T>* writer_;  // not owned by the class.
  T (*f_)(const T&);
  std::function<T(const T&)> g_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_FILTERED_SERVER_WRITER_WRAPPER_H_
