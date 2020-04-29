/*
 * Copyright 2020-present Open Networking Foundation
 * Copyright 2020 PLVision
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

#ifndef STRATUM_HAL_LIB_PHAL_OPTICS_ADAPTER_H_
#define STRATUM_HAL_LIB_PHAL_OPTICS_ADAPTER_H_

#include <memory>
#include <thread>  // NOLINT
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/adapter.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {
namespace phal {

class OpticsAdapter final : public Adapter {
 public:
  explicit OpticsAdapter(AttributeDatabaseInterface* attribute_db_interface);

  ~OpticsAdapter() = default;

  // Gets the information about an optical transceiver module by querying the
  // Phal database.
  // See: PhalInterface::GetOpticalTransceiverInfo.
  ::util::Status GetOpticalTransceiverInfo(int slot, int port,
                                           OpticalChannelInfo* oc_info);

  // Sets the data from oc_info into an optical transceiver module in the Phal
  // database.
  // See: PhalInterface::SetOpticalTransceiverInfo.
  ::util::Status SetOpticalTransceiverInfo(int slot, int port,
                                           const OpticalChannelInfo& oc_info);

 private:
  // Attribute Db path to get the hardware state of all sfp transceivers.
  const Path kAllOpticsPath = {
      PathEntry("optical_cards", -1, true, true, false),
      PathEntry("hardware_state", -1, false, true, false),
  };

  // Mutex guarding internal state.
  absl::Mutex subscribers_lock_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_OPTICS_ADAPTER_H_
