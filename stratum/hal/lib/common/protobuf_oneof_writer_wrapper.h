// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_WRITER_INTERFACE_H_
#define STRATUM_HAL_LIB_COMMON_WRITER_INTERFACE_H_

#include <memory>
#include <utility>

#include "stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {

// Wrapper for WriterInterface class which constrains the allowed protobuf
// message type to one specific embedded message. It can be used when we have a
// channel for a generic message with embedded oneof submessages and want to
// restrict write access to only one specific oneof message. This allows using
// the same channel across different writers, while maintaining type safety
// without the need for extra channels and threads.
template <typename T, typename R>
class ConstraintWriterWrapper : public WriterInterface<R> {
 public:
  explicit ConstraintWriterWrapper(std::shared_ptr<WriterInterface<T>> writer,
                                   R* (T::*get_mutable_inner_message)())
      : writer_(std::move(writer)),
        get_mutable_inner_message_(get_mutable_inner_message) {}
  bool Write(const R& msg) override {
    if (!writer_) return false;
    T t;
    *(t.*get_mutable_inner_message_)() = msg;
    return writer_->Write(t);
  }

 private:
  std::shared_ptr<WriterInterface<T>> writer_;
  R* (T::*get_mutable_inner_message_)();
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_WRITER_INTERFACE_H_
