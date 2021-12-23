// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_STRINGSOURCE_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_STRINGSOURCE_INTERFACE_H_

#include <string>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {

// StringSourceInterface provides a single interface for all system operations
// that return a std::string (e.g. file access, simple scripts, EEPROM access).
class StringSourceInterface {
 public:
  virtual ~StringSourceInterface() {}
  // Performs whatever operations necessary to produce and return a new
  // std::string. Returns failure if a string cannot be produced for any
  // reason. There are no limits on execution time.
  virtual ::util::StatusOr<std::string> GetString() = 0;
  // Performs whatever operations necessary to write the given string to this
  // string source. Returns failure if such a write is not permitted or fails
  // for any reason. There are no limits on execution time.
  virtual ::util::Status SetString(const std::string& buffer) = 0;
  // Returns true if calls to SetString are valid. This does not guarantee
  // that they *will* succeed, only that they *might* succeed.
  virtual bool CanSet() = 0;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_STRINGSOURCE_INTERFACE_H_
