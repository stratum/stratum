// Copyright 2018-present Open Networking Foundation
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


#ifndef STRATUM_HAL_LIB_DUMMY_DUMMY_CHASSIS_MGR_H_
#define STRATUM_HAL_LIB_DUMMY_DUMMY_CHASSIS_MGR_H_

#include <vector>
#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/dummy/dummy_box.h"
#include "stratum/hal/lib/dummy/dummy_global_vars.h"

namespace stratum {
namespace hal {
namespace dummy {

using Request = stratum::hal::DataRequest::Request;

class DummyChassisManager {
 public:
  ~DummyChassisManager();
  // Update chassis configuration.
  ::util::Status PushChassisConfig(const ChassisConfig& config)
  EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Verify chassis configuration.
  ::util::Status VerifyChassisConfig(const ChassisConfig& config)
  SHARED_LOCKS_REQUIRED(chassis_lock);

  // Shutdown the chassis.
  ::util::Status Shutdown()
  EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Freeze and Unfreeze the chassis.
  ::util::Status Freeze() EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);
  ::util::Status Unfreeze() EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // There should be only one chassis manager in a physical device.
  static DummyChassisManager* GetSingleton();

  // Register/Unregister event notifier
  ::util::Status RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer)
  SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::Status UnregisterEventNotifyWriter()
  SHARED_LOCKS_REQUIRED(chassis_lock);

  // Retrieves chassis data
  ::util::StatusOr<DataResponse>
  RetrieveChassisData(const Request request);

  // DummyChassisManager is neither copyable nor movable.
  DummyChassisManager(const DummyChassisManager&) = delete;
  DummyChassisManager& operator=(const DummyChassisManager&) = delete;
  DummyChassisManager(DummyChassisManager&&) = delete;
  DummyChassisManager& operator=(DummyChassisManager&&) = delete;

 private:
  // Hide default constructor
  DummyChassisManager();
  std::shared_ptr<WriterInterface<GnmiEventPtr>> chassis_event_writer_;
  DummyBox* dummy_box_;
};

}  // namespace dummy
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_DUMMY_DUMMY_CHASSIS_MGR_H_
