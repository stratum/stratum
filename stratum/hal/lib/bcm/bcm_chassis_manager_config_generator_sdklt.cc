// Copyright 2020 Open Networking Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"

#include <algorithm>
#include <set>
#include <sstream>  // IWYU pragma: keep

#include "gflags/gflags.h"
#include "google/protobuf/message.h"
#include "stratum/glue/integral_types.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/bcm/utils.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"

DECLARE_string(bcm_sdk_config_file);

namespace stratum {
namespace hal {
namespace bcm {

::util::Status BcmChassisManager::WriteBcmConfigFile(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map) const {
  // TODO(Yi): Implement this method.
  std::stringstream buffer;

  return ::util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
