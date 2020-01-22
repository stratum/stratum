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


#include "stratum/hal/lib/tai/types_converter.h"

namespace stratum {
namespace hal {
namespace tai {

/*!
 * \brief TypesConverter::HertzToMegahertz method converts \param hertz to
 * megahertz
 */
::google::protobuf::uint64 TypesConverter::HertzToMegahertz(
    tai_uint64_t hertz) {
  return hertz / kMegahertzInHertz;
}

/*!
 * \brief TypesConverter::MegahertzToHertz method converts \param megahertz to
 * hertz
 */
tai_uint64_t TypesConverter::MegahertzToHertz(
    ::google::protobuf::uint64 megahertz) {
  return megahertz * kMegahertzInHertz;
}

}  // namespace tai
}  // namespace hal
}  // namespace stratum
