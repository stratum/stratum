// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_LIB_CHANNEL_CHANNEL_MOCK_H_
#define STRATUM_LIB_CHANNEL_CHANNEL_MOCK_H_

#include <memory>
#include <vector>

#include "stratum/lib/channel/channel.h"
#include "gmock/gmock.h"

namespace stratum {

template <typename T>
class ChannelMock : public Channel<T> {
 public:
  explicit ChannelMock(size_t max_depth) : Channel<T>(max_depth) {}
  MOCK_METHOD0_T(IsClosed, bool());
  MOCK_METHOD0_T(Close, bool());
  MOCK_METHOD2_T(Read, ::util::Status(T* t, absl::Duration timeout));
  MOCK_METHOD1_T(TryRead, ::util::Status(T* t));
  MOCK_METHOD1_T(ReadAll, ::util::Status(std::vector<T>* t_s));
  MOCK_METHOD2_T(Write, ::util::Status(const T& t, absl::Duration timeout));
  MOCK_METHOD2_T(Write, ::util::Status(T&& t, absl::Duration timeout));
  MOCK_METHOD1_T(TryWrite, ::util::Status(const T& t));
  MOCK_METHOD1_T(TryWrite, ::util::Status(T&& t));
  MOCK_METHOD2_T(
      SelectRegister,
      void(const std::shared_ptr<channel_internal::SelectData>& select_data,
           bool* t_ready));
};

template <typename T>
class ChannelReaderMock : public ChannelReader<T> {
 public:
  MOCK_METHOD2_T(Read, ::util::Status(T* t, absl::Duration timeout));
  MOCK_METHOD1_T(TryRead, ::util::Status(T* t));
  MOCK_METHOD1_T(ReadAll, ::util::Status(std::vector<T>* t_s));
  MOCK_METHOD0_T(IsClosed, bool());
};

template <typename T>
class ChannelWriterMock : public ChannelWriter<T> {
 public:
  MOCK_METHOD2_T(Write, ::util::Status(const T& t, absl::Duration timeout));
  MOCK_METHOD2_T(Write, ::util::Status(T&& t, absl::Duration timeout));
  MOCK_METHOD1_T(TryWrite, ::util::Status(const T& t));
  MOCK_METHOD1_T(TryWrite, ::util::Status(T&& t));
  MOCK_METHOD0_T(IsClosed, bool());
};

}  // namespace stratum

#endif  // STRATUM_LIB_CHANNEL_CHANNEL_MOCK_H_
