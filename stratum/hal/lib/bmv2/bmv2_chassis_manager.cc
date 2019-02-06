/* Copyright 2019-present Barefoot Networks, Inc.
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

#include "stratum/hal/lib/bmv2/bmv2_chassis_manager.h"

#include "bm/bm_sim/dev_mgr.h"
#include "bm/simple_switch/runner.h"

#include <functional>  // std::bind
#include <map>
#include <memory>
#include <utility>  // std::pair

#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/glue/integral_types.h"
#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace bmv2 {

absl::Mutex chassis_lock;

Bmv2ChassisManager::Bmv2ChassisManager(
    PhalInterface* phal_interface,
    std::map<uint64, ::bm::sswitch::SimpleSwitchRunner*> node_id_to_bmv2_runner)
    : phal_interface_(phal_interface),
      node_id_to_bmv2_runner_(node_id_to_bmv2_runner),
      node_id_to_bmv2_port_status_cb_(),
      node_id_to_port_id_to_port_state_() {
  for (auto& node : node_id_to_bmv2_runner) {
    CHECK_EQ(node.first, node.second->get_device_id())
        << "Device / node id mismatch with bmv2 runner";
  }
  RegisterEventWriters();
}

Bmv2ChassisManager::~Bmv2ChassisManager() = default;

::util::Status Bmv2ChassisManager::PushChassisConfig(
     const ChassisConfig& config) {
  // TODO(antonin): handle singleton ports and do the appropriate port adds /
  // deletes with the appropriate bmv2 runner.
  return ::util::OkStatus();
}

::util::Status Bmv2ChassisManager::VerifyChassisConfig(
     const ChassisConfig& config) {
  return ::util::OkStatus();
}

::util::Status Bmv2ChassisManager::RegisterEventNotifyWriter(
    const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status Bmv2ChassisManager::UnregisterEventNotifyWriter() {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = nullptr;
  return ::util::OkStatus();
}

::util::StatusOr<PortState> Bmv2ChassisManager::GetPortState(
    uint64 node_id, uint32 port_id) {
  auto* port_id_to_port_state =
      gtl::FindOrNull(node_id_to_port_id_to_port_state_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_port_state != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const PortState* port_state_ptr =
      gtl::FindOrNull(*port_id_to_port_state, port_id);
  // TODO(antonin): Once we implement PushChassisConfig, port_state_ptr should
  // never be NULL
  if (port_state_ptr != nullptr && *port_state_ptr != PORT_STATE_UNKNOWN) {
    return *port_state_ptr;
  }

  // If state is unknown, query the state
  LOG(INFO) << "Querying state of port " << port_id << " in node " << node_id
            << " with bmv2";
  // should not be NULL because we already validated node_id by looking it up in
  // node_id_to_port_id_to_port_state_
  auto runner = gtl::FindPtrOrNull(node_id_to_bmv2_runner_, node_id);
  if (runner == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "No bmv2 runner for node id " << node_id
                                    << ".";
  }
  auto dev_mgr = runner->get_dev_mgr();
  auto is_up = dev_mgr->port_is_up(
      static_cast<bm::PortMonitorIface::port_t>(port_id));
  PortState port_state = is_up ? PORT_STATE_UP : PORT_STATE_DOWN;
  LOG(INFO) << "State of port " << port_id << " in node " << node_id << ": "
            << PrintPortState(port_state);
  return port_state;
}

::util::Status Bmv2ChassisManager::GetPortCounters(
    uint64 node_id, uint32 port_id, PortCounters* counters) {
  auto* runner = gtl::FindOrNull(node_id_to_bmv2_runner_, node_id);
  CHECK_RETURN_IF_FALSE(runner != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  auto dev_mgr = (*runner)->get_dev_mgr();
  auto port_stats = dev_mgr->get_port_stats(
      static_cast<bm::PortMonitorIface::port_t>(port_id));
  counters->set_in_octets(port_stats.in_octets);
  counters->set_out_octets(port_stats.out_octets);
  counters->set_in_unicast_pkts(port_stats.in_packets);
  counters->set_out_unicast_pkts(port_stats.out_packets);
  // we explicitly set these to 0 (even though it is not required with proto3)
  // to show the reader which stats we are not supporting
  counters->set_in_broadcast_pkts(0);
  counters->set_out_broadcast_pkts(0);
  counters->set_in_multicast_pkts(0);
  counters->set_out_multicast_pkts(0);
  counters->set_in_discards(0);
  counters->set_out_discards(0);
  counters->set_in_unknown_protos(0);
  counters->set_in_errors(0);
  counters->set_out_errors(0);
  counters->set_in_fcs_errors(0);
  return ::util::OkStatus();
}

std::unique_ptr<Bmv2ChassisManager> Bmv2ChassisManager::CreateInstance(
    PhalInterface* phal_interface,
    std::map<uint64, ::bm::sswitch::SimpleSwitchRunner*>
      node_id_to_bmv2_runner) {
  return absl::WrapUnique(new Bmv2ChassisManager(
      phal_interface, node_id_to_bmv2_runner));
}

void Bmv2ChassisManager::SendPortOperStateGnmiEvent(
    uint64 node_id, uint64 port_id, PortState new_state) {
  absl::ReaderMutexLock l(&gnmi_event_lock_);
  if (!gnmi_event_writer_) return;
  // Allocate and initialize a PortOperStateChangedEvent event and pass it to
  // the gNMI publisher using the gNMI event notification channel.
  // The GnmiEventPtr is a smart pointer (shared_ptr<>) and it takes care of
  // the memory allocated to this event object once the event is handled by
  // the GnmiPublisher.
  if (!gnmi_event_writer_->Write(GnmiEventPtr(
          new PortOperStateChangedEvent(node_id, port_id, new_state)))) {
    // Remove WriterInterface if it is no longer operational.
    gnmi_event_writer_.reset();
  }
}

::util::Status PortStatusChangeCb(Bmv2ChassisManager* chassis_manager,
                                  uint64 node_id,
                                  uint64 port_id,
                                  PortState new_state) {
  LOG(INFO) << "State of port " << port_id << " in node " << node_id << ": "
            << PrintPortState(new_state);
  absl::WriterMutexLock l(&chassis_lock);
  chassis_manager->node_id_to_port_id_to_port_state_[node_id][port_id] =
      new_state;
  chassis_manager->SendPortOperStateGnmiEvent(
      node_id, port_id, new_state);
  return ::util::OkStatus();
}

namespace {

void PortStatusChangeCbInternal(Bmv2ChassisManager* chassis_manager,
                                uint64 node_id,
                                bm::PortMonitorIface::port_t port,
                                bm::PortMonitorIface::PortStatus status) {
  PortState state;
  if (status == bm::PortMonitorIface::PortStatus::PORT_UP) {
    state = PORT_STATE_UP;
  } else if (status == bm::PortMonitorIface::PortStatus::PORT_DOWN) {
    state = PORT_STATE_DOWN;
  } else {
    LOG(ERROR) << "Invalid port state CB from bmv2 for node " << node_id << ".";
  }
  PortStatusChangeCb(
      chassis_manager, node_id, static_cast<uint64>(port), state);
}

}  // namespace

::util::Status Bmv2ChassisManager::RegisterEventWriters() {
  using PortStatus = bm::DevMgr::PortStatus;
  for (auto& node : node_id_to_bmv2_runner_) {
    auto runner = node.second;
    auto dev_mgr = runner->get_dev_mgr();
    auto cb = std::bind(PortStatusChangeCbInternal, this, node.first,
                        std::placeholders::_1, std::placeholders::_2);
    auto p = node_id_to_bmv2_port_status_cb_.emplace(node.first, cb);
    auto success = p.second;
    CHECK(success) << "Port status CB already registered for node "
                   << node.first;
    auto& cb_ref = p.first->second;
    dev_mgr->register_status_cb(PortStatus::PORT_UP, cb_ref);
    dev_mgr->register_status_cb(PortStatus::PORT_DOWN, cb_ref);
  }
  LOG(INFO) << "Port status notification callback registered successfully.";
  return ::util::OkStatus();
}

::util::Status Bmv2ChassisManager::UnregisterEventWriters() {
  return ::util::OkStatus();
}

}  // namespace bmv2
}  // namespace hal
}  // namespace stratum
