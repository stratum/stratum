// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_WRITER_INTERFACE_H_
#define STRATUM_HAL_LIB_COMMON_WRITER_INTERFACE_H_

#include <memory>
#include <utility>

namespace stratum {
namespace hal {

// An interface for a wrapper around the Write operation for various data
// transport mechanisms, e.g. Stratum Channel, gRPC ServerWriter. This class
// makes the SwitchInterface class more abstract and eliminates the dependency
// to gRPC as well as Stratum-internal constructs.
template <typename T>
class WriterInterface {
 public:
  virtual ~WriterInterface() {}
  // Blocking Write() operation which passes a message of type T into the
  // underlying transfer mechanism.
  virtual bool Write(const T& msg) = 0;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  WriterInterface() {}
};

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
