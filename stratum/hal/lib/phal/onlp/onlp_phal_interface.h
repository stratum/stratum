// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PHAL_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PHAL_INTERFACE_H_

#include "stratum/hal/lib/phal/phal_backend_interface.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/onlp/onlp_event_handler.h"
#include "stratum/hal/lib/phal/sfp_adapter.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

// TODO(Yi-Tseng): We don't support multiple slot for now,
// use slot 1 as default slot.
constexpr int kDefaultSlot = 1;

// Class "OnlpPhalInterface" defines the Onlp Phal backend interface.
class OnlpPhalInterface : public PhalBackendInterface {
 public:
  // Register a OnlpEventCallback
  virtual ::util::Status RegisterOnlpEventCallback(
      OnlpEventCallback* callback) = 0;
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PHAL_INTERFACE_H_
