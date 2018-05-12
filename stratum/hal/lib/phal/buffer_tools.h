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


#ifndef STRATUM_HAL_LIB_PHAL_BUFFER_TOOLS_H_
#define STRATUM_HAL_LIB_PHAL_BUFFER_TOOLS_H_

#include <cstddef>

#include "absl/base/integral_types.h"

namespace stratum {
namespace hal {
namespace phal {

// Helper functions for parsing fields from char buffers.

// Reads the given number of bytes from source and interprets them as an
// unsigned integral value of the given type.
template <typename T>
static T ParseIntegralBytes(const char* source, size_t num_bytes,
                            bool little_endian) {
  T number = 0;
  for (size_t i = 0; i < num_bytes; ++i) {
    uint8 byte;
    if (little_endian)
      byte = static_cast<uint8>(source[num_bytes - i - 1]);
    else
      byte = static_cast<uint8>(source[i]);

    number |= static_cast<T>(byte) << ((num_bytes - i - 1) * 8);
  }
  return number;
}

// Reads the given number of bytes from source and interprets them as a signed
// integral value of the given type. The highest order bit is always interpreted
// as the sign bit.
template <typename T>
static T ParseSignedIntegralBytes(const char* source, size_t num_bytes,
                                  bool little_endian) {
  T value = ParseIntegralBytes<T>(source, num_bytes, little_endian);
  // If the number should be negative, fix its value.
  if (num_bytes < sizeof(T) && value & (1 << (8 * num_bytes - 1))) {
    // This is an incomplete 1's complement value. Prepend 1's
    // up to the length of the type.
    return value | (~0 << (8 * num_bytes));
  } else {
    return value;
  }
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_BUFFER_TOOLS_H_
