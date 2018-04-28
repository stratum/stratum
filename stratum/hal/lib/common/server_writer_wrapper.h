/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef STRATUM_HAL_LIB_COMMON_SERVER_WRITER_WRAPPER_H_
#define STRATUM_HAL_LIB_COMMON_SERVER_WRITER_WRAPPER_H_

#include <grpcpp/grpcpp.h>
#include "third_party/stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {

// Wrapper for ::grpc::ServerWriter based on WriterInterface class.
template <typename T>
class ServerWriterWrapper : public WriterInterface<T> {
 public:
  explicit ServerWriterWrapper(::grpc::ServerWriter<T>* writer)
      : writer_(writer) {}
  bool Write(const T& msg) override {
    if (writer_) return writer_->Write(msg);
    return false;
  }

 private:
  ::grpc::ServerWriter<T>* writer_;  // not owned by the class.
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_SERVER_WRITER_WRAPPER_H_
