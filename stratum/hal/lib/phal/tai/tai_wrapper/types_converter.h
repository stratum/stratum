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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TYPES_CONVERTER_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TYPES_CONVERTER_H_

#include <string>
#include <utility>
#include <vector>

#include "external/com_github_oopt_tai/inc/tainetworkif.h"
#include "external/com_github_oopt_tai/inc/taitypes.h"
#include "stratum/glue/integral_types.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief The TypesConverter class should take responsibility for the
 * transformation of gNMI values to TAI and vice-versa
 * \note class is designed to be just a methods/constants container and can't be
 * inherited
 */
class TypesConverter final {
 public:
  TypesConverter() = delete;

  static uint64 HertzToMegahertz(tai_uint64_t hertz);
  static tai_uint64_t MegahertzToHertz(uint64 megahertz);
  static tai_int32_t OperationalModeToModulation(uint64 operational_mode);
  static uint64 ModulationToOperationalMode(tai_int32_t modulation);

 private:
  static constexpr uint kMegahertzInHertz = 1000000;
  static const std::vector<std::pair<uint64, tai_int32_t>>
      kOperationalModeToModulation;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TYPES_CONVERTER_H_
