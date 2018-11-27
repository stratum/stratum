/* Copyright 2018-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_CHASSIS_MANAGER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_CHASSIS_MANAGER_H_

#include <map>
#include <memory>

#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/glue/integral_types.h"
#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BFChassisManager {
 public:
  virtual ~BFChassisManager();

  virtual ::util::Status RegisterEventNotifyWriter(
      const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer)
      LOCKS_EXCLUDED(gnmi_event_lock_);

  virtual ::util::Status UnregisterEventNotifyWriter()
      LOCKS_EXCLUDED(gnmi_event_lock_);

  virtual ::util::StatusOr<PortState> GetPortState(
      uint64 node_id, uint32 port_id)
      LOCKS_EXCLUDED(chassis_config_lock_);

  virtual ::util::Status GetPortCounters(
      uint64 node_id, uint32 port_id, PortCounters* counters);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BFChassisManager> CreateInstance(
      PhalInterface* phal_interface,
      const std::map<int, uint64>& unit_to_node_id);

  // BFChassisManager is neither copyable nor movable.
  BFChassisManager(const BFChassisManager&) = delete;
  BFChassisManager& operator=(const BFChassisManager&) = delete;
  BFChassisManager(BFChassisManager&&) = delete;
  BFChassisManager& operator=(BFChassisManager&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BFChassisManager(PhalInterface* phal_interface,
                   const std::map<int, uint64>& unit_to_node_id);

  ::util::Status RegisterEventWriters();
  ::util::Status UnregisterEventWriters();

  // Forward PortStatus changed events through the appropriate node's registered
  // ChannelWriter<GnmiEventPtr> object.
  void SendPortOperStateGnmiEvent(uint64 node_id, uint64 port_id,
                                  PortState new_state)
      LOCKS_EXCLUDED(gnmi_event_lock_);

  friend ::util::Status PortStatusChangeCb(int unit, uint64 port_id,
                                           bool up, void *cookie);

  // WriterInterface<GnmiEventPtr> object for sending event notifications.
  mutable absl::Mutex gnmi_event_lock_;
  std::shared_ptr<WriterInterface<GnmiEventPtr>> gnmi_event_writer_
      GUARDED_BY(gnmi_event_lock_);

  // Pointer to a PhalInterface implementation.
  PhalInterface* phal_interface_;  // not owned by this class.

  // Map from unit number to the node ID as specified by the config.
  std::map<int, uint64> unit_to_node_id_;

  // Map from node ID to unit number.
  std::map<uint64, int> node_id_to_unit_;

  // Protects config and oper state for chassis.
  mutable absl::Mutex chassis_config_lock_;

  // Map from node ID to another map from port ID to PortState representing
  // the state of the singleton port uniquely identified by (node ID, port ID).
  std::map<uint64, std::map<uint32, PortState>>
      node_id_to_port_id_to_port_state_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_CHASSIS_MANAGER_H_
