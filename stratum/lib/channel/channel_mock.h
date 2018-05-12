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


#ifndef STRATUM_LIB_CHANNEL_CHANNEL_MOCK_H_
#define STRATUM_LIB_CHANNEL_CHANNEL_MOCK_H_

#include "stratum/lib/channel/channel.h"
#include "testing/base/public/gmock.h"

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
