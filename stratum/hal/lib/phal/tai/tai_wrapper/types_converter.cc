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

#include "stratum/hal/lib/phal/tai/tai_wrapper/types_converter.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

const std::vector<std::pair<uint64, tai_int32_t>>
    TypesConverter::kOperationalModeToModulation = {
        {1, TAI_NETWORK_INTERFACE_MODULATION_FORMAT_DP_QPSK},
        {2, TAI_NETWORK_INTERFACE_MODULATION_FORMAT_DP_16_QAM},
        {3, TAI_NETWORK_INTERFACE_MODULATION_FORMAT_DP_8_QAM},
};

/*!
 * \brief TypesConverter::HertzToMegahertz method converts \param hertz to
 * megahertz
 */
uint64 TypesConverter::HertzToMegahertz(tai_uint64_t hertz) {
  return hertz / kMegahertzInHertz;
}

/*!
 * \brief TypesConverter::MegahertzToHertz method converts \param megahertz to
 * hertz
 */
tai_uint64_t TypesConverter::MegahertzToHertz(uint64 megahertz) {
  return megahertz * kMegahertzInHertz;
}

/*!
 * \brief TypesConverter::OperationalModeToModulation method converts
 * \param operational_mode to modulation.
 * \return a corresponding modulation if found or
 * TAI_NETWORK_INTERFACE_MODULATION_FORMAT_UNKNOWN otherwise (or if the
 * operational mode is equal to zero).
 */
tai_int32_t TypesConverter::OperationalModeToModulation(
    uint64 operational_mode) {
  for (const auto& kv : kOperationalModeToModulation) {
    if (kv.first == operational_mode) return kv.second;
  }
  return TAI_NETWORK_INTERFACE_MODULATION_FORMAT_UNKNOWN;
}

/*!
 * \brief TypesConverter::ModulationToOperationalMode method converts
 * \param modulation to operational mode.
 * \return a corresponding operational mode if found or zero otherwise (or if
 * the modulation is equal to TAI_NETWORK_INTERFACE_MODULATION_FORMAT_UNKNOWN).
 */
uint64 TypesConverter::ModulationToOperationalMode(tai_int32_t modulation) {
  for (const auto& kv : kOperationalModeToModulation) {
    if (kv.second == modulation) return kv.first;
  }
  return 0;
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
