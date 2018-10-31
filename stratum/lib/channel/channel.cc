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

#include "stratum/lib/channel/channel.h"
#include "absl/synchronization/mutex.h"

namespace stratum {

using channel_internal::ChannelBase;
using channel_internal::SelectData;

::util::StatusOr<SelectResult> Select(const std::vector<ChannelBase*>& channels,
                                      absl::Duration timeout) {
  // Create output map;
  auto ready_flags =
      absl::make_unique<std::unordered_map<ChannelBase*, bool>>();
  // Create and initialize management object.
  auto select_data = std::make_shared<SelectData>();
  select_data->done = false;
  // Register operation on all of the given Channels.
  for (auto& channel : channels) {
    bool& flag = (*ready_flags)[channel] = false;
    channel->SelectRegister(select_data, &flag);
  }
  absl::Time deadline = absl::Now() + timeout;
  absl::MutexLock l(&select_data->lock);
  // Wait with timeout until one or more Channels signal data available to read.
  while (!select_data->done) {
    // Check if all Channels involved have been closed, in which case there
    // would be no signal.
    if (select_data.use_count() == 1) {
      return MAKE_ERROR(ERR_CANCELLED) << "All Channels have been closed.";
    }
    bool expired =
        select_data->cond.WaitWithDeadline(&select_data->lock, deadline);
    // If the timer expired without the operation completing, return failure.
    if (expired && !select_data->done) {
      return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
             << "Read did not succeed within timeout due to empty "
             << "Channel(s).";
    }
  }
  return SelectResult(std::move(ready_flags));
}

}  // namespace stratum
