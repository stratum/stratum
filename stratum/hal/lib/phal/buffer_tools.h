// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_BUFFER_TOOLS_H_
#define STRATUM_HAL_LIB_PHAL_BUFFER_TOOLS_H_

#include <cstddef>

#include "stratum/glue/integral_types.h"

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
