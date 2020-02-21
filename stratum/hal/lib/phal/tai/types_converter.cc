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

#include "stratum/hal/lib/phal/tai/types_converter.h"

#include "absl/strings/numbers.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

const std::vector<std::pair<uint64, std::string>>
    TypesConverter::kOperationalModeToModulation = {
        {1, "dp-qpsk"},
        {2, "dp-16-qam"},
        {3, "dp-8-qam"},
};

/*!
 * \brief TypesConverter::HertzToMegahertz method converts \param hertz to
 * megahertz
 */
uint64 TypesConverter::HertzToMegahertz(const std::string hertz) {
  uint64 u_hertz;
  CHECK(absl::SimpleAtoi(hertz, &u_hertz))
      << "Could not convert " << hertz << " to uint64.";
  return u_hertz / kMegahertzInHertz;
}

/*!
 * \brief TypesConverter::MegahertzToHertz method converts \param megahertz to
 * hertz
 */
std::string TypesConverter::MegahertzToHertz(uint64 megahertz) {
  return std::to_string(megahertz * kMegahertzInHertz);
}

/*!
 * \brief TypesConverter::OperationalModeToModulation method converts
 * \param operational_mode to modulation.
 * \return a corresponding modulation if found or
 * TAI_NETWORK_INTERFACE_MODULATION_FORMAT_UNKNOWN otherwise (or if the
 * operational mode is equal to zero).
 */
std::string TypesConverter::OperationalModeToModulation(
    uint64 operational_mode) {
  for (const auto& kv : kOperationalModeToModulation) {
    if (kv.first == operational_mode) return kv.second;
  }
  return {};
}

/*!
 * \brief TypesConverter::ModulationToOperationalMode method converts
 * \param modulation to operational mode.
 * \return a corresponding operational mode if found or zero otherwise (or if
 * the modulation is equal to TAI_NETWORK_INTERFACE_MODULATION_FORMAT_UNKNOWN).
 */
uint64 TypesConverter::ModulationToOperationalMode(
    const std::string& modulation) {
  for (const auto& kv : kOperationalModeToModulation) {
    if (kv.second == modulation) return kv.first;
  }
  return 0;
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
