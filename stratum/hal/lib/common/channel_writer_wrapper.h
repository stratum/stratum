// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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
