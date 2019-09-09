/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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


#ifndef STRATUM_HAL_LIB_COMMON_CHANNEL_WRITER_WRAPPER_H_
#define STRATUM_HAL_LIB_COMMON_CHANNEL_WRITER_WRAPPER_H_

#include <memory>
#include <utility>

#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {

// Wrapper for ChannelWriter class which implements WriterInterface.
template <typename T>
class ChannelWriterWrapper : public WriterInterface<T> {
 public:
  explicit ChannelWriterWrapper(std::unique_ptr<ChannelWriter<T>> writer)
      : writer_(std::move(writer)) {}
  bool Write(const T& msg) override {
    if (!writer_) return false;
    auto status = writer_->Write(msg, absl::InfiniteDuration());
    if (!status.ok()) {
      VLOG(3) << "Unable to write to Channel with error code: "
              << status.error_code() << ".";
      return false;
    }
    return true;
  }

 private:
  std::unique_ptr<ChannelWriter<T>> writer_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_CHANNEL_WRITER_WRAPPER_H_
