// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_FIXED_STRINGSOURCE_H_
#define STRATUM_HAL_LIB_PHAL_FIXED_STRINGSOURCE_H_

#include <string>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/macros.h"
#include "stratum/hal/lib/phal/stringsource_interface.h"

namespace stratum {
namespace hal {
namespace phal {

// A StringSource that produces a single fixed value.
class FixedStringSource : public StringSourceInterface {
 public:
  // Constructs a FixedStringSource that will always produce the given string.
  // If can_set == true, SetString will overwrite the stored fixed string.
  explicit FixedStringSource(const std::string& fixed_string)
      : fixed_string_(fixed_string) {}
  ::util::StatusOr<std::string> GetString() override { return fixed_string_; }
  ::util::Status SetString(const std::string& buffer) override {
    return MAKE_ERROR() << "Attempted to set a FixedStringSource.";
  }
  bool CanSet() override { return false; }

 private:
  std::string fixed_string_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_FIXED_STRINGSOURCE_H_
