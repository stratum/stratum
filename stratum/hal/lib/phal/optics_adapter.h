// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
// SPDX-License-Identifier: Apache-2.0

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
  ::util::Status GetOpticalTransceiverInfo(int32 module, int network_interface,
                                           OpticalTransceiverInfo* ot_info);

  // Sets the data from ot_info into an optical transceiver module in the Phal
  // database.
  // See: PhalInterface::SetOpticalTransceiverInfo.
  ::util::Status
  SetOpticalTransceiverInfo(int32 module, int network_interface,
                            const OpticalTransceiverInfo& ot_info);

 private:
  // Attribute Db path to get the hardware state of all sfp transceivers.
  const Path kAllOpticsPath = {
      PathEntry("optical_modules", -1, true, true, false),
  };

  // Mutex guarding internal state.
  absl::Mutex subscribers_lock_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_OPTICS_ADAPTER_H_
