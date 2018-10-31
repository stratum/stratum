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

#ifndef STRATUM_LIB_CHANNEL_CHANNEL_INTERNAL_H_
#define STRATUM_LIB_CHANNEL_CHANNEL_INTERNAL_H_

#include "stratum/lib/macros.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace channel_internal {

// Data used by a Channel to manage an ongoing Select operation.
struct SelectData {
  absl::Mutex lock;
  absl::CondVar cond;
  bool done;
};

// Non-templated base Channel class. This exists to facilitate operations on
// Channels which are agnostic of the message type.
class ChannelBase {
 public:
  virtual ~ChannelBase() {}

  // Registers a Select operation on this Channel. Returns false if the Channel
  // is closed.
  virtual void SelectRegister(const std::shared_ptr<SelectData>& select_data,
                              bool* ready) = 0;

  // Disallow copy and assign.
  ChannelBase(const ChannelBase&) = delete;
  ChannelBase& operator=(const ChannelBase&) = delete;

 protected:
  // Default constructor.
  ChannelBase() {}
};

}  // namespace channel_internal
}  // namespace stratum

#endif  // STRATUM_LIB_CHANNEL_CHANNEL_INTERNAL_H_
