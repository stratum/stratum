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


#ifndef STRATUM_HAL_LIB_PHAL_FILEPATH_STRINGSOURCE_H_
#define STRATUM_HAL_LIB_PHAL_FILEPATH_STRINGSOURCE_H_

#include <string>

#include "third_party/stratum/glue/status/status_macros.h"
#include "third_party/stratum/glue/status/statusor.h"
#include "third_party/stratum/glue/status/status.h"
#include "third_party/stratum/hal/lib/phal/stringsource_interface.h"
#include "third_party/stratum/hal/lib/phal/system_interface.h"
#include "third_party/stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace phal {

// A StringSource that produces the contents of a filepath.
class FilepathStringSource : public StringSourceInterface {
 public:
  // Constructs a FilepathStringSource that uses the given system interface to
  // read the contents of the given filepath. If can_set == true, SetString
  // will use system_interface to write to the given filepath.
  FilepathStringSource(const SystemInterface* system_interface,
                       const std::string& filepath, bool can_set)
      : system_interface_(system_interface),
        filepath_(filepath),
        can_set_(can_set) {}
  // Constructs a FilepathStringSource that uses the given system interface to
  // read the contents of the given filepath. This string source does not
  // support SetString().
  FilepathStringSource(const SystemInterface* system_interface,
                       const std::string& filepath)
      : FilepathStringSource(system_interface, filepath, false) {}
  ::util::StatusOr<std::string> GetString() override {
    std::string output;
    RETURN_IF_ERROR(system_interface_->ReadFileToString(filepath_, &output));
    return output;
  }
  ::util::Status SetString(const std::string& buffer) override {
    if (can_set_) {
      return system_interface_->WriteStringToFile(buffer, filepath_);
    } else {
      RETURN_ERROR() << "Attempted to set an unsettable FilepathStringSource.";
    }
  }
  bool CanSet() override {
    return can_set_;
  }

 private:
  const SystemInterface* system_interface_;
  std::string filepath_;
  bool can_set_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_FILEPATH_STRINGSOURCE_H_
