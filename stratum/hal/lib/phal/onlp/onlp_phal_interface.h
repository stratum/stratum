/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

// TODO
// Class "OnlpPhal" is an implementation of PhalInterface which is used to
// send the OnlpPhal events to Stratum.
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
