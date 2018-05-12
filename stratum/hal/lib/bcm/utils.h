/*
 * Copyright 2018 Google LLC
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


#ifndef STRATUM_HAL_LIB_BCM_UTILS_H_
#define STRATUM_HAL_LIB_BCM_UTILS_H_

#include <string>

#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "absl/base/integral_types.h"

namespace stratum {
namespace hal {
namespace bcm {

// Prints a BcmPort message in a consistent and readable format. There are two
// versions for this function, one taking port_id as well (if available).
std::string PrintBcmPort(const BcmPort& p);
std::string PrintBcmPort(uint64 port_id, const BcmPort& p);

// Prints BcmPortOptions message in a consistent and readable format.
std::string PrintBcmPortOptions(const BcmPortOptions& options);

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_UTILS_H_
