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

#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"

#include <map>
#include <memory>

#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/hal/lib/barefoot/bf_pal_interface.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/glue/integral_types.h"
#include "stratum/lib/channel/channel.h"
#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"

namespace stratum {
namespace hal {
namespace barefoot {

using PortStatusChangeEvent = BFPalInterface::PortStatusChangeEvent;

absl::Mutex chassis_lock;

/* static */
constexpr int BFChassisManager::kMaxPortStatusChangeEventDepth;

BFChassisManager::BFChassisManager(PhalInterface* phal_interface,
                                   BFPalInterface* bf_pal_interface)
    : initialized_(false),
      port_status_change_event_channel_(nullptr),
      phal_interface_(phal_interface),
      bf_pal_interface_(bf_pal_interface),
      unit_to_node_id_(),
      node_id_to_unit_(),
      node_id_to_port_id_to_port_state_() {}

BFChassisManager::~BFChassisManager() = default;

::util::Status BFChassisManager::AddPortHelper(
     uint64 node_id, int unit, uint32 port_id,
     const SingletonPort& singleton_port /* desired config */,
     /* out */ PortConfig* config /* new config */) {
  config->admin_state = ADMIN_STATE_UNKNOWN;

  const auto& config_params = singleton_port.config_params();
  if (config_params.admin_state() == ADMIN_STATE_UNKNOWN) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Invalid admin state for port " << port_id << " in node " << node_id;
  }
  if (config_params.admin_state() == ADMIN_STATE_DIAG) {
    RETURN_ERROR(ERR_UNIMPLEMENTED)
        << "Unsupported 'diags' admin state for port " << port_id
        << " in node " << node_id;
  }

  LOG(INFO) << "Adding port " << port_id << " in node " << node_id << ".";
  RETURN_IF_ERROR(bf_pal_interface_->PortAdd(
      unit, port_id, singleton_port.speed_bps()));
  config->speed_bps = singleton_port.speed_bps();
  config->admin_state = ADMIN_STATE_DISABLED;

  if (config_params.mtu() != 0) {
    RETURN_IF_ERROR(bf_pal_interface_->PortMtuSet(
        unit, port_id, config_params.mtu()));
  }
  config->mtu = config_params.mtu();
  if (config_params.autoneg() != TRI_STATE_UNKNOWN) {
    RETURN_IF_ERROR(bf_pal_interface_->PortAutonegPolicySet(
        unit, port_id, config_params.autoneg()));
  }
  config->autoneg = config_params.autoneg();
  if (config_params.admin_state() == ADMIN_STATE_ENABLED) {
    LOG(INFO) << "Enabling port " << port_id << " in node " << node_id << ".";
    RETURN_IF_ERROR(bf_pal_interface_->PortEnable(unit, port_id));
    config->admin_state = ADMIN_STATE_ENABLED;
  }

  return ::util::OkStatus();
}

::util::Status BFChassisManager::UpdatePortHelper(
     uint64 node_id, int unit, uint32 port_id,
     const SingletonPort& singleton_port /* desired config */,
     const PortConfig& config_old /* current config */,
     /* out */ PortConfig* config /* new config */) {
  *config = config_old;

  if (!bf_pal_interface_->PortIsValid(unit, port_id)) {
    config->admin_state = ADMIN_STATE_UNKNOWN;
    config->speed_bps.reset();
    RETURN_ERROR(ERR_INTERNAL)
        << "Port " << port_id << " in node " << node_id << " is not valid.";
  }

  // we do not support changing the speed (even for channel 0), ask the client
  // to remove the port and add it again
  if (singleton_port.speed_bps() != config_old.speed_bps) {
    RETURN_ERROR(ERR_UNIMPLEMENTED)
        << "The speed for port " << port_id << " in node " << node_id
        << " has changed; you need to delete the port and add it again.";
  }

  const auto& config_params = singleton_port.config_params();
  if (config_params.admin_state() == ADMIN_STATE_UNKNOWN) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Invalid admin state for port " << port_id << " in node " << node_id;
  }
  if (config_params.admin_state() == ADMIN_STATE_DIAG) {
    RETURN_ERROR(ERR_UNIMPLEMENTED)
        << "Unsupported 'diags' admin state for port " << port_id
        << " in node " << node_id;
  }

  bool config_changed = false;

  if (config_params.mtu() != config_old.mtu) {
    VLOG(1) << "Mtu for port " << port_id << " in node " << node_id
            << " changed.";
    config->mtu.reset();
    RETURN_IF_ERROR(bf_pal_interface_->PortMtuSet(
        unit, port_id, config_params.mtu()));
    config->mtu = config_params.mtu();
    config_changed = true;
  }
  if (config_params.autoneg() != config_old.autoneg) {
    VLOG(1) << "Autoneg policy for port " << port_id << " in node " << node_id
            << " changed.";
    config->autoneg.reset();
    RETURN_IF_ERROR(bf_pal_interface_->PortAutonegPolicySet(
        unit, port_id, config_params.autoneg()));
    config->autoneg = config_params.autoneg();
    config_changed = true;
  }

  bool need_disable = false, need_enable = false;
  if (config_params.admin_state() == ADMIN_STATE_DISABLED) {
    // if the new admin state is disabled, we need to disable the port if it was
    // previously enabled.
    need_disable = (config_old.admin_state != ADMIN_STATE_DISABLED);
  } else if (config_params.admin_state() == ADMIN_STATE_ENABLED) {
    // if the new admin state is enabled, we need to:
    //  * disable the port if there is a config chaned and the port was
    //    previously enabled
    //  * enable the port if it needs to be disabled first because of a config
    //    change if it is currently disabled
    need_disable =
        config_changed && (config_old.admin_state != ADMIN_STATE_DISABLED);
    need_enable =
        need_disable || (config_old.admin_state == ADMIN_STATE_DISABLED);
  }

  if (need_disable) {
    LOG(INFO) << "Disabling port " << port_id << " in node " << node_id << ".";
    RETURN_IF_ERROR(bf_pal_interface_->PortDisable(unit, port_id));
    config->admin_state = ADMIN_STATE_DISABLED;
  }
  if (need_enable) {
    LOG(INFO) << "Enabling port " << port_id << " in node " << node_id << ".";
    RETURN_IF_ERROR(bf_pal_interface_->PortEnable(unit, port_id));
    config->admin_state = ADMIN_STATE_ENABLED;
  }

  return ::util::OkStatus();
}

::util::Status BFChassisManager::PushChassisConfig(
     const ChassisConfig& config) {
  if (!initialized_) RETURN_IF_ERROR(RegisterEventWriters());

  // new maps
  std::map<int, uint64> unit_to_node_id;
  std::map<uint64, int> node_id_to_unit;
  std::map<uint64, std::map<uint32, PortState>>
      node_id_to_port_id_to_port_state;
  std::map<uint64, std::map<uint32, PortConfig>>
      node_id_to_port_id_to_port_config;

  int unit(0);
  for (auto& node : config.nodes()) {
    unit_to_node_id[unit] = node.id();
    node_id_to_unit[node.id()] = unit;
    unit++;
  }

  for (auto singleton_port : config.singleton_ports()) {
    uint32 port_id = singleton_port.id();
    uint64 node_id = singleton_port.node();

    auto* unit = gtl::FindOrNull(node_id_to_unit, node_id);
    if (unit == nullptr) {
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "Invalid ChassisConfig, unknown node id " << node_id
          << " for port " << port_id << ".";
    }
    node_id_to_port_id_to_port_state[node_id][port_id] = PORT_STATE_UNKNOWN;
    node_id_to_port_id_to_port_config[node_id][port_id] = PortConfig();
  }

  ::util::Status status = ::util::OkStatus();  // errors to keep track of.

  for (auto singleton_port : config.singleton_ports()) {
    uint32 port_id = singleton_port.id();
    uint64 node_id = singleton_port.node();
    // we checked that node_id was valid in the previous loop
    auto unit = node_id_to_unit[node_id];

    // TODO(antonin): we currently ignore slot, port and channel (note that
    // Stratum requires slot and port to be set). We require id to be set to the
    // Tofino device port.

    const PortConfig* config_old = nullptr;
    const auto* port_id_to_port_config_old = gtl::FindOrNull(
        node_id_to_port_id_to_port_config_, node_id);
    if (port_id_to_port_config_old != nullptr) {
      config_old = gtl::FindOrNull(*port_id_to_port_config_old, port_id);
    }

    auto& config = node_id_to_port_id_to_port_config[node_id][port_id];
    if (config_old == nullptr) {  // new port
      // if anything fails, config.admin_state will be set to
      // ADMIN_STATE_UNKNOWN (invalid)
      APPEND_STATUS_IF_ERROR(
          status,
          AddPortHelper(node_id, unit, port_id, singleton_port, &config));
    } else {  // port already exists, config may have changed
      if (config_old->admin_state == ADMIN_STATE_UNKNOWN) {
        // something is wrong with the port, we make sure the port is deleted
        // first (and ignore the error status if there is one), then add the
        // port again.
        if (bf_pal_interface_->PortIsValid(unit, port_id)) {
          bf_pal_interface_->PortDelete(unit, port_id);
        }
        APPEND_STATUS_IF_ERROR(
            status,
            AddPortHelper(node_id, unit, port_id, singleton_port, &config));
        continue;
      }

      // diff configs and apply necessary changes

      // sanity-check: if admin_state is not ADMIN_STATE_UNKNOWN, then the port
      // was added and the speed_bps was set.
      if (!config_old->speed_bps) {
        RETURN_ERROR(ERR_INTERNAL)
            << "Invalid internal state in BFChassisManager, "
            << "speed_bps field should contain a value";
      }

      // if anything fails, config.admin_state will be set to
      // ADMIN_STATE_UNKNOWN (invalid)
      APPEND_STATUS_IF_ERROR(
          status,
          UpdatePortHelper(
              node_id, unit, port_id, singleton_port, *config_old, &config));
    }
  }

  for (const auto& node_ports_old : node_id_to_port_id_to_port_config_) {
    auto node_id = node_ports_old.first;
    for (const auto& port_old : node_ports_old.second) {
      auto port_id = port_old.first;
      if (node_id_to_port_id_to_port_config.count(node_id) > 0 &&
          node_id_to_port_id_to_port_config[node_id].count(port_id) > 0) {
        continue;
      }
      // remove ports which are no longer present in the ChassisConfig
      LOG(INFO) << "Deleting port " << port_id << " in node " << node_id << ".";
      APPEND_STATUS_IF_ERROR(
          status, bf_pal_interface_->PortDelete(node_id, port_id));
    }
  }

  unit_to_node_id_ = unit_to_node_id;
  node_id_to_unit_ = node_id_to_unit;
  node_id_to_port_id_to_port_state_ = node_id_to_port_id_to_port_state;
  node_id_to_port_id_to_port_config_ = node_id_to_port_id_to_port_config;
  initialized_ = true;

  return status;
}

::util::Status BFChassisManager::VerifyChassisConfig(
     const ChassisConfig& config) {
  return ::util::OkStatus();
}

::util::Status BFChassisManager::RegisterEventNotifyWriter(
    const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status BFChassisManager::UnregisterEventNotifyWriter() {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = nullptr;
  return ::util::OkStatus();
}

::util::StatusOr<const BFChassisManager::PortConfig*>
BFChassisManager::GetPortConfig(uint64 node_id, uint32 port_id) const {
  auto* port_id_to_config =
      gtl::FindOrNull(node_id_to_port_id_to_port_config_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_config != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const PortConfig* config = gtl::FindOrNull(*port_id_to_config, port_id);
  CHECK_RETURN_IF_FALSE(config != nullptr)
      << "Port " << port_id << " is not configured or not known for node "
      << node_id << ".";
  return config;
}

::util::StatusOr<DataResponse> BFChassisManager::GetPortData(
     const DataRequest::Request& request) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  DataResponse resp;
  using Request = DataRequest::Request;
  switch (request.request_case()) {
    case Request::kOperStatus: {
      ASSIGN_OR_RETURN(auto port_state, GetPortState(
          request.oper_status().node_id(), request.oper_status().port_id()));
      resp.mutable_oper_status()->set_state(port_state);
      break;
    }
    case Request::kAdminStatus: {
      ASSIGN_OR_RETURN(auto* config, GetPortConfig(
          request.admin_status().node_id(), request.admin_status().port_id()));
      resp.mutable_admin_status()->set_state(config->admin_state);
      break;
    }
    case Request::kPortSpeed: {
      ASSIGN_OR_RETURN(auto* config, GetPortConfig(
          request.port_speed().node_id(), request.port_speed().port_id()));
      if (config->speed_bps)
        resp.mutable_port_speed()->set_speed_bps(*config->speed_bps);
      break;
    }
    case Request::kNegotiatedPortSpeed: {
      ASSIGN_OR_RETURN(auto* config, GetPortConfig(
          request.port_speed().node_id(), request.port_speed().port_id()));
      if (!config->speed_bps) break;
      ASSIGN_OR_RETURN(auto port_state, GetPortState(
          request.oper_status().node_id(), request.oper_status().port_id()));
      if (port_state != PORT_STATE_UP) break;
      resp.mutable_negotiated_port_speed()->set_speed_bps(*config->speed_bps);
      break;
    }
    case Request::kPortCounters: {
      RETURN_IF_ERROR(GetPortCounters(
          request.port_counters().node_id(),
          request.port_counters().port_id(),
          resp.mutable_port_counters()));
      break;
    }
    case Request::kAutonegStatus: {
      ASSIGN_OR_RETURN(auto* config, GetPortConfig(
          request.port_speed().node_id(), request.port_speed().port_id()));
      if (config->autoneg)
        resp.mutable_autoneg_status()->set_state(*config->autoneg);
      break;
    }
    default:
      RETURN_ERROR(ERR_INTERNAL) << "Not supported yet";
  }
  return resp;
}

::util::StatusOr<PortState> BFChassisManager::GetPortState(
    uint64 node_id, uint32 port_id) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const int* unit = gtl::FindOrNull(node_id_to_unit_, node_id);
  if (unit == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Unkonwn node id " << node_id;
  }

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
            << ".";
  ASSIGN_OR_RETURN(auto port_state,
                   bf_pal_interface_->PortOperStateGet(*unit, port_id));
  LOG(INFO) << "State of port " << port_id << " in node " << node_id << ": "
            << PrintPortState(port_state);
  return port_state;
}

::util::Status BFChassisManager::GetPortCounters(
    uint64 node_id, uint32 port_id, PortCounters* counters) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const int* unit = gtl::FindOrNull(node_id_to_unit_, node_id);
  if (unit == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Unkonwn node id " << node_id;
  }
  return bf_pal_interface_->PortAllStatsGet(*unit, port_id, counters);
  return ::util::OkStatus();
}

::util::StatusOr<std::map<uint64, int>> BFChassisManager::GetNodeIdToUnitMap()
    const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  return node_id_to_unit_;
}

::util::Status BFChassisManager::ReplayPortsConfig(uint64 node_id) {
  auto* unit_ptr = gtl::FindOrNull(node_id_to_unit_, node_id);
  if (unit_ptr == nullptr) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Unknown node id " << node_id << ".";
  }
  int unit = *unit_ptr;

  for (auto& p : node_id_to_port_id_to_port_state_[node_id])
    p.second = PORT_STATE_UNKNOWN;

  LOG(INFO) << "Replaying ports for node " << node_id << ".";

  auto replay_one_port = [node_id, unit, this](
      uint32 port_id, const PortConfig& config, PortConfig* config_new)
      -> ::util::Status {
    VLOG(1) << "Replaying port " << port_id << " in node " << node_id << ".";

    if (config.admin_state == ADMIN_STATE_UNKNOWN) {
      LOG(WARNING) << "Port " << port_id << " in node " << node_id
                   << " was not configured properly, so skipping replay.";
      return ::util::OkStatus();
    }

    if (!config.speed_bps) {
      RETURN_ERROR(ERR_INTERNAL)
          << "Invalid internal state in BFChassisManager, "
          << "speed_bps field should contain a value";
    }

    RETURN_IF_ERROR(bf_pal_interface_->PortAdd(
        unit, port_id, *config.speed_bps));
    config_new->speed_bps = *config.speed_bps;
    config_new->admin_state = ADMIN_STATE_DISABLED;

    if (config.mtu) {
      RETURN_IF_ERROR(bf_pal_interface_->PortMtuSet(
          unit, port_id, *config.mtu));
      config_new->mtu = *config.mtu;
    }
    if (config.autoneg) {
      RETURN_IF_ERROR(bf_pal_interface_->PortAutonegPolicySet(
          unit, port_id, *config.autoneg));
      config_new->autoneg = *config.autoneg;
    }

    if (config.admin_state == ADMIN_STATE_ENABLED) {
      VLOG(1) << "Enabling port " << port_id << " in node " << node_id << ".";
      RETURN_IF_ERROR(bf_pal_interface_->PortEnable(unit, port_id));
      config_new->admin_state = ADMIN_STATE_ENABLED;
    }

    return ::util::OkStatus();
  };

  ::util::Status status = ::util::OkStatus();  // errors to keep track of.

  for (auto& p : node_id_to_port_id_to_port_config_[node_id]) {
    PortConfig config_new;
    APPEND_STATUS_IF_ERROR(
        status, replay_one_port(p.first, p.second, &config_new));
    p.second = config_new;
  }

  return status;
}

::util::Status BFChassisManager::ResetPortsConfig(uint64 node_id) {
  auto* port_id_to_config =
      gtl::FindOrNull(node_id_to_port_id_to_port_config_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_config != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  auto* port_id_to_state =
      gtl::FindOrNull(node_id_to_port_id_to_port_state_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_state != nullptr)
      << "Node " << node_id << " has a configuration mismatch.";
  for (auto& p : *port_id_to_config)
    p.second = PortConfig();
  for (auto& p : *port_id_to_state)
    p.second = PORT_STATE_UNKNOWN;
  return ::util::OkStatus();
}

std::unique_ptr<BFChassisManager> BFChassisManager::CreateInstance(
    PhalInterface* phal_interface, BFPalInterface* bf_pal_interface) {
  return absl::WrapUnique(new BFChassisManager(
      phal_interface, bf_pal_interface));
}

void BFChassisManager::SendPortOperStateGnmiEvent(
    uint64 node_id, uint32 port_id, PortState new_state) {
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

void BFChassisManager::ReadPortStatusChangeEvents() {
  PortStatusChangeEvent event;
  while (true) {
    // port_status_change_event_reader_ does not need to be protected by a mutex
    // because this thread is the only one accessing it. It is assigned in
    // RegisterEventWriters and then left untouched until UnregisterEventWriters
    // is called. UnregisterEventWriters joins this thread before resetting the
    // reader.
    int code = port_status_change_event_reader_->Read(
        &event, absl::InfiniteDuration()).error_code();
    // Exit if the Channel is closed.
    if (code == ERR_CANCELLED) break;
    // Read should never timeout.
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }
    // Handle received message.
    {
      absl::WriterMutexLock l(&chassis_lock);
      const uint64* node_id = gtl::FindOrNull(unit_to_node_id_, event.unit);
      if (node_id == nullptr) {
        LOG(ERROR) << "Unkonwn unit / device id " << event.unit << ".";
        continue;
      }
      auto* state = gtl::FindOrNull(node_id_to_port_id_to_port_state_[*node_id],
                                    event.port_id);
      if (state == nullptr) {
        LOG(ERROR) << "Unknown port " << event.port_id << " in node "
                   << *node_id << ".";
        continue;
      }
      LOG(INFO) << "State of port " << event.port_id << " in node " << *node_id
                << ": " << PrintPortState(event.state);
      *state = event.state;
      SendPortOperStateGnmiEvent(*node_id, event.port_id, event.state);
    }
  }
}

::util::Status BFChassisManager::RegisterEventWriters() {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "RegisterEventWriters() can be called only before the class is "
           << "initialized.";
  }

  port_status_change_event_channel_ = Channel<PortStatusChangeEvent>::Create(
      kMaxPortStatusChangeEventDepth);
  // Create and hand-off Writer to the BFPalInterface.
  auto writer = ChannelWriter<PortStatusChangeEvent>::Create(
      port_status_change_event_channel_);
  RETURN_IF_ERROR(bf_pal_interface_->PortStatusChangeRegisterEventWriter(
      std::move(writer)));
  LOG(INFO) << "Port status notification callback registered successfully";

  port_status_change_event_reader_ =
      ChannelReader<PortStatusChangeEvent>::Create(
          port_status_change_event_channel_);
  port_status_change_event_thread_ = std::thread(
      [this]() { this->ReadPortStatusChangeEvents(); });

  return ::util::OkStatus();
}

::util::Status BFChassisManager::UnregisterEventWriters() {
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(
      status, bf_pal_interface_->PortStatusChangeUnregisterEventWriter());
  if (!port_status_change_event_channel_->Close()) {
    APPEND_ERROR(status)
        << "Error when closing port status change event channel.";
  }
  port_status_change_event_thread_.join();
  // Once the thread is joined, it is safe to reset these pointers.
  port_status_change_event_reader_ = nullptr;
  port_status_change_event_channel_ = nullptr;
  return status;
}

::util::Status BFChassisManager::Shutdown() {
  ::util::Status status = ::util::OkStatus();
  {
    absl::ReaderMutexLock l(&chassis_lock);
    if (!initialized_) return status;
  }
  // It is fine to release the chassis lock here (it is actually needed to call
  // UnregisterEventWriters or there would be a deadlock). Because initialized_
  // is set to true, RegisterEventWriters cannot be called.
  APPEND_STATUS_IF_ERROR(status, UnregisterEventWriters());
  {
    absl::WriterMutexLock l(&chassis_lock);
    initialized_ = false;
  }
  return status;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
