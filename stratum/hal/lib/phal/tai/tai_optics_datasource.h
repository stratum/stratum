/*
 * Copyright 2020-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_OPTICS_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_OPTICS_DATASOURCE_H_

#include <vector>
#include <memory>
#include <string>

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

class TaiOpticsDataSource : public DataSource {
 public:
  // TODO(plvision): Add doc string
  static ::util::StatusOr<std::shared_ptr<TaiOpticsDataSource>> Make(
      int id, TaiInterface* Tai_interface, CachePolicy* cache_policy);

  // Accessors for managed attributes.
  ManagedAttribute* GetOpticsId() { return id_attribute_.get(); }

 protected:
  // Protected constructor.
  TaiOpticsDataSource(int id, TaiInterface* Tai_interface,
                    CachePolicy* cache_policy);

 private:
  ::util::Status UpdateValues() override;

  // Pointer to the Tai interface. Not created or owned by this class.
  TaiInterface* tai_interface_;

  // Managed attributes.
  std::unique_ptr<TypedAttribute<int>> id_attribute_;
  std::unique_ptr<TypedAttribute<bool>> present_attribute_;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_OPTICS_DATASOURCE_H_
