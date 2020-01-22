/*
 * Copyright 2019 Dell EMC
 * Copyright 2019-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_SFP_ADAPTER_H_
#define STRATUM_HAL_LIB_PHAL_SFP_ADAPTER_H_

#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/phal/adapter.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/managed_attribute.h"

namespace stratum {
namespace hal {
namespace phal {

class SfpAdapter : public Adapter {
 public:
  explicit SfpAdapter(AttributeDatabaseInterface* attribute_db_interface);

  SfpAdapter() = default;

  virtual ~SfpAdapter() = default;

  // Slot and port are 1-based.
  ::util::Status GetFrontPanelPortInfo(int slot, int port,
                                       FrontPanelPortInfo* fp_port_info);
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SFP_ADAPTER_H_
