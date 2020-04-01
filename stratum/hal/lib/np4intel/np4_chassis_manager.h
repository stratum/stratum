/* Copyright 2019-present Barefoot Networks, Inc.
 * Copyright 2019-present Dell EMC
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

#ifndef STRATUM_HAL_LIB_NP4INTEL_NP4_CHASSIS_MANAGER_H_
#define STRATUM_HAL_LIB_NP4INTEL_NP4_CHASSIS_MANAGER_H_

#include <map>
#include <memory>
#include <utility>
#include <string>
#include <thread>  // NOLINT

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/glue/integral_types.h"
#include "stratum/lib/channel/channel.h"
#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace np4intel {

// Lock which protects chassis state across the entire switch.
extern absl::Mutex chassis_lock;

class NP4ChassisManager {
 public:
  virtual ~NP4ChassisManager();

  virtual ::util::Status PushChassisConfig(const ChassisConfig& config)
      EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  virtual ::util::Status Shutdown() LOCKS_EXCLUDED(chassis_lock);

  virtual ::util::Status RegisterEventNotifyWriter(
      const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer)
      LOCKS_EXCLUDED(gnmi_event_lock_);

  virtual ::util::Status UnregisterEventNotifyWriter()
      LOCKS_EXCLUDED(gnmi_event_lock_);

  virtual ::util::StatusOr<DataResponse> GetPortData(
      const DataRequest::Request& request)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  virtual ::util::StatusOr<PortState> GetPortState(
      uint64 node_id, uint32 port_id)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  virtual ::util::Status GetPortCounters(
      uint64 node_id, uint32 port_id, PortCounters* counters)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<NP4ChassisManager> CreateInstance(
      PhalInterface* phal_interface);

  // NP4ChassisManager is neither copyable nor movable.
  NP4ChassisManager(const NP4ChassisManager&) = delete;
  NP4ChassisManager& operator=(const NP4ChassisManager&) = delete;
  NP4ChassisManager(NP4ChassisManager&&) = delete;
  NP4ChassisManager& operator=(NP4ChassisManager&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  NP4ChassisManager(
      PhalInterface* phal_interface);

  ::util::Status RegisterEventWriters()
        EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);
  ::util::Status UnregisterEventWriters()
        EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Cleans up the internal state. Resets all the internal port maps and
  // deletes the pointers.
  void CleanupInternalState() EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Forward PortStatus changed events through the appropriate node's registered
  // ChannelWriter<GnmiEventPtr> object.
  void SendPortOperStateGnmiEvent(uint64 node_id, uint32 port_id,
                                  PortState new_state)
      LOCKS_EXCLUDED(gnmi_event_lock_);

  // Thread function for reading and processing port state events.
  void ReadPortStatusChangeEvents() LOCKS_EXCLUDED(chassis_lock);

  ::util::StatusOr<const SingletonPort*> GetSingletonPort(
      uint64 node_id, uint32 port_id) const
      SHARED_LOCKS_REQUIRED(chassis_lock);

  bool initialized_ GUARDED_BY(chassis_lock);

  // Pointer to a PhalInterface implementation.
  PhalInterface* phal_interface_;  // not owned by this class.

  // WriterInterface<GnmiEventPtr> object for sending event notifications.
  mutable absl::Mutex gnmi_event_lock_;
  std::shared_ptr<WriterInterface<GnmiEventPtr>> gnmi_event_writer_
      GUARDED_BY(gnmi_event_lock_);

  // Maximum depth of port status change event channel.
  static constexpr int kMaxPortStatusChangeEventDepth = 1024;

  struct PortStatusChangeEvent {
    uint64 node_id;
    uint32 port_id;
    PortState state;
  };

  // A Channel to enable asynchronous processing of the port events generated by
  // np4. This is the safe way to process these events, as np4 may generate a
  // callback synchronously during a port add operation, and the risk of
  // deadlock is high...
  std::shared_ptr<Channel<PortStatusChangeEvent> >
      port_status_change_event_channel_ GUARDED_BY(chassis_lock);

  std::unique_ptr<ChannelReader<PortStatusChangeEvent> >
      port_status_change_event_reader_;

  std::unique_ptr<ChannelWriter<PortStatusChangeEvent> >
      port_status_change_event_writer_
      GUARDED_BY(port_status_change_event_writer_lock_);

  std::thread port_status_change_event_thread_;

  mutable absl::Mutex port_status_change_event_writer_lock_;

  // Map from node ID to another map from port ID to PortState representing
  // the state of the singleton port uniquely identified by (node ID, port ID).
  std::map<uint64, std::map<uint32, PortState>>
      node_id_to_port_id_to_port_state_ GUARDED_BY(chassis_lock);

  // Map from node ID to another map from port ID to SignletonPort representing
  // the config of the singleton port uniquely identified by (node ID, port ID).
  std::map<uint64, std::map<uint32, SingletonPort>>
      node_id_to_port_id_to_port_config_ GUARDED_BY(chassis_lock);
};

}  // namespace np4intel
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_NP4INTEL_NP4_CHASSIS_MANAGER_H_
