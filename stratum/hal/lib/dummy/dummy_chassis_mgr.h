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
#include "stratum/glue/gtl/flat_hash_map.h"
#include "stratum/hal/lib/dummy/dummy_node.h"
#include "stratum/hal/lib/dummy/dummy_sdk.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

class DummyChassisManager {
 public:
  ~DummyChassisManager();
  // Update chassis configuration.
  // The chassis manager should also update the nodes based on
  // the config.
  ::util::Status PushChassisConfig(const ChassisConfig& config)
  EXCLUSIVE_LOCKS_REQUIRED(chassis_lock_);

  // Verify chassis configuration but not update the node.
  ::util::Status VerifyChassisConfig(const ChassisConfig& config)
  SHARED_LOCKS_REQUIRED(chassis_lock_);

  // Shutdown the chassis.
  ::util::Status Shutdown()
  EXCLUSIVE_LOCKS_REQUIRED(chassis_lock_);

  // Freeze and Unfreeze the chassis.
  // Every public method call to the freezed node should be hanged
  // or returns an error state with proper message.
  ::util::Status Freeze() EXCLUSIVE_LOCKS_REQUIRED(chassis_lock_);
  ::util::Status Unfreeze() EXCLUSIVE_LOCKS_REQUIRED(chassis_lock_);

  // There should be only one chassis manager in a physical device.
  static DummyChassisManager* GetSingleton()
  SHARED_LOCKS_REQUIRED(chassis_lock_);

  // Get a DummyNode based on the Id.
  ::util::StatusOr<DummyNode*> GetDummyNode(uint64 node_id)
  SHARED_LOCKS_REQUIRED(chassis_lock_);

  // Get all DummyNodes.
  std::vector<DummyNode*> GetDummyNodes()
  SHARED_LOCKS_REQUIRED(chassis_lock_);

  // Register/Unregister event notifier
  ::util::Status RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer)
  EXCLUSIVE_LOCKS_REQUIRED(gnmi_event_lock_);
  ::util::Status UnregisterEventNotifyWriter()
  EXCLUSIVE_LOCKS_REQUIRED(gnmi_event_lock_);

  // Retrieve value from a specific node.
  ::util::Status RetrieveValue(uint64 node_id, const DataRequest& requests,
                               WriterInterface<DataResponse>* writer,
                               std::vector<::util::Status>* details)
  SHARED_LOCKS_REQUIRED(chassis_lock_);

  // DummyChassisManager is neither copyable nor movable.
  DummyChassisManager(const DummyChassisManager&) = delete;
  DummyChassisManager& operator=(const DummyChassisManager&) = delete;
  DummyChassisManager(DummyChassisManager&&) = delete;
  DummyChassisManager& operator=(DummyChassisManager&&) = delete;

 private:
  // Hide default constructor
  DummyChassisManager();

  // Retrieves chassis data
  ::util::StatusOr<DataResponse>
  RetrieveChassisData(const Request request);

  std::shared_ptr<WriterInterface<GnmiEventPtr>> chassis_event_writer_;

  // Maps to hold nodes.
  ::stratum::gtl::flat_hash_map<int, DummyNode*> dummy_nodes_;
  DummySDK* dummy_sdk_;
};

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_DUMMY_DUMMY_CHASSIS_MGR_H_
