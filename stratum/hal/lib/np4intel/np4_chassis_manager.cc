// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2019-present Dell EMC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/np4intel/np4_chassis_manager.h"

#include <functional>  // std::bind
#include <map>
#include <memory>
#include <utility>  // std::pair

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "stratum/glue/integral_types.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace np4intel {

constexpr int NP4ChassisManager::kMaxPortStatusChangeEventDepth;

absl::Mutex chassis_lock;

NP4ChassisManager::NP4ChassisManager(PhalInterface* phal_interface)
    : initialized_(false),
      phal_interface_(phal_interface),
      port_status_change_event_channel_(nullptr),
      port_status_change_event_reader_(nullptr),
      port_status_change_event_writer_(nullptr),
      node_id_to_port_id_to_port_state_(),
      node_id_to_port_id_to_port_config_() {}

NP4ChassisManager::~NP4ChassisManager() = default;

namespace {

// helper to add a np4intel port
::util::Status AddPort(uint64 node_id, const std::string& port_name,
                       uint32 port_id) {
  LOG(INFO) << "Adding port " << port_id << " to node " << node_id;

  return ::util::OkStatus();
}

// helper to remove a np4intel port
::util::Status RemovePort(uint64 node_id, uint32 port_id) {
  LOG(INFO) << "Removing port " << port_id << " from node " << node_id;

  return ::util::OkStatus();
}

}  // namespace

::util::Status NP4ChassisManager::PushChassisConfig(
    const ChassisConfig& config) {
  VLOG(1) << "NP4ChassisManager::PushChassisConfig";
  ::util::Status status = ::util::OkStatus();  // errors to keep track of.

  if (!initialized_) RETURN_IF_ERROR(RegisterEventWriters());

  // build new maps
  std::map<uint64, std::map<uint32, PortState>>
      node_id_to_port_id_to_port_state;
  std::map<uint64, std::map<uint32, SingletonPort>>
      node_id_to_port_id_to_port_config;
  for (auto singleton_port : config.singleton_ports()) {
    uint32 port_id = singleton_port.id();
    uint64 node_id = singleton_port.node();
    node_id_to_port_id_to_port_state[node_id][port_id] = PORT_STATE_UNKNOWN;
    node_id_to_port_id_to_port_config[node_id][port_id] = singleton_port;
  }

  // Compare ports in old config and new config and perform the necessary
  // operations.
  for (auto& node : config.nodes()) {
    VLOG(1) << "Updating config for node " << node.id() << ".";

    // Remove or change existing port config
    for (const auto& port_old : node_id_to_port_id_to_port_config_[node.id()]) {
      auto port_id = port_old.first;
      auto* singleton_port = gtl::FindOrNull(
          node_id_to_port_id_to_port_config[node.id()], port_id);

      if (singleton_port == nullptr) {  // remove port if not present any more
        auto& config_old = port_old.second.config_params();
        if (config_old.admin_state() == ADMIN_STATE_ENABLED) {
          APPEND_STATUS_IF_ERROR(status, RemovePort(node.id(), port_id));
        }
      } else {  // change port config if needed
        auto& config_old = port_old.second.config_params();
        auto& config = singleton_port->config_params();
        if (config.admin_state() != config_old.admin_state()) {
          if (config.admin_state() == ADMIN_STATE_ENABLED) {
            APPEND_STATUS_IF_ERROR(
                status, AddPort(node.id(), singleton_port->name(), port_id));
          } else {
            APPEND_STATUS_IF_ERROR(status, RemovePort(node.id(), port_id));
            if (node_id_to_port_id_to_port_state_[node.id()][port_id] ==
                PORT_STATE_UP) {
              // TODO(antonin): would it be better to just register a bmv2
              // callback for PORT_REMOVED event?
              VLOG(1) << "Sending DOWN notification for port " << port_id
                      << " in node " << node.id() << ".";
              SendPortOperStateGnmiEvent(node.id(), port_id, PORT_STATE_DOWN);
            }
          }
        }
      }
    }

    // Add a new port config
    for (const auto& port : node_id_to_port_id_to_port_config[node.id()]) {
      auto port_id = port.first;
      auto* singleton_port_old = gtl::FindOrNull(
          node_id_to_port_id_to_port_config_[node.id()], port_id);

      if (singleton_port_old == nullptr) {  // add new port
        auto& singleton_port = port.second;
        auto& config = singleton_port.config_params();
        if (config.admin_state() == ADMIN_STATE_ENABLED) {
          APPEND_STATUS_IF_ERROR(
              status, AddPort(node.id(), singleton_port.name(), port_id));
        } else {
          LOG(INFO) << "Port " << port_id
                    << " is listed in ChassisConfig for node " << node.id()
                    << " but its admin state is not set to enabled.";
        }
      }
    }
  }

  node_id_to_port_id_to_port_state_ = node_id_to_port_id_to_port_state;
  node_id_to_port_id_to_port_config_ = node_id_to_port_id_to_port_config;
  initialized_ = true;

  return status;
}

::util::Status NP4ChassisManager::VerifyChassisConfig(
    const ChassisConfig& config) {
  return ::util::OkStatus();
}

::util::Status NP4ChassisManager::RegisterEventNotifyWriter(
    const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status NP4ChassisManager::UnregisterEventNotifyWriter() {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = nullptr;
  return ::util::OkStatus();
}

::util::StatusOr<const SingletonPort*> NP4ChassisManager::GetSingletonPort(
    uint64 node_id, uint32 port_id) const {
  auto* port_id_to_singleton =
      gtl::FindOrNull(node_id_to_port_id_to_port_config_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_singleton != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const SingletonPort* singleton =
      gtl::FindOrNull(*port_id_to_singleton, port_id);
  CHECK_RETURN_IF_FALSE(singleton != nullptr)
      << "Port " << port_id << " is not configured or not known for node "
      << node_id << ".";
  return singleton;
}

::util::StatusOr<DataResponse> NP4ChassisManager::GetPortData(
    const DataRequest::Request& request) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  DataResponse resp;
  using Request = DataRequest::Request;
  switch (request.request_case()) {
    case Request::kOperStatus: {
      ASSIGN_OR_RETURN(auto port_state,
                       GetPortState(request.oper_status().node_id(),
                                    request.oper_status().port_id()));
      resp.mutable_oper_status()->set_state(port_state);
      break;
    }
    case Request::kAdminStatus: {
      ASSIGN_OR_RETURN(auto* singleton,
                       GetSingletonPort(request.admin_status().node_id(),
                                        request.admin_status().port_id()));
      resp.mutable_admin_status()->set_state(
          singleton->config_params().admin_state());
      break;
    }
    case Request::kPortSpeed: {
      ASSIGN_OR_RETURN(auto* singleton,
                       GetSingletonPort(request.port_speed().node_id(),
                                        request.port_speed().port_id()));
      resp.mutable_port_speed()->set_speed_bps(singleton->speed_bps());
      break;
    }
    case Request::kNegotiatedPortSpeed: {
      ASSIGN_OR_RETURN(
          auto* singleton,
          GetSingletonPort(request.negotiated_port_speed().node_id(),
                           request.negotiated_port_speed().port_id()));
      resp.mutable_negotiated_port_speed()->set_speed_bps(
          singleton->speed_bps());
      break;
    }
    case Request::kPortCounters: {
      RETURN_IF_ERROR(GetPortCounters(request.port_counters().node_id(),
                                      request.port_counters().port_id(),
                                      resp.mutable_port_counters()));
      break;
    }
    case Request::kAutonegStatus: {
      ASSIGN_OR_RETURN(auto* singleton,
                       GetSingletonPort(request.autoneg_status().node_id(),
                                        request.autoneg_status().port_id()));
      resp.mutable_autoneg_status()->set_state(
          singleton->config_params().autoneg());
      break;
    }
    default:
      RETURN_ERROR(ERR_INTERNAL) << "Not supported yet";
  }
  return resp;
}

::util::StatusOr<PortState> NP4ChassisManager::GetPortState(uint64 node_id,
                                                            uint32 port_id) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  auto* port_id_to_port_state =
      gtl::FindOrNull(node_id_to_port_id_to_port_state_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_port_state != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const PortState* port_state_ptr =
      gtl::FindOrNull(*port_id_to_port_state, port_id);
  CHECK_RETURN_IF_FALSE(port_state_ptr != nullptr)
      << "Port " << port_id << " is not configured or not known for node "
      << node_id << ".";
  if (*port_state_ptr != PORT_STATE_UNKNOWN) return *port_state_ptr;

  // If state is unknown, query the state
  LOG(INFO) << "Querying state of port " << port_id << " in node " << node_id
            << " with np4intel";
  // should not be NULL because we already validated node_id by looking it up in
  // node_id_to_port_id_to_port_state_

  // TODO(craig): Need to fix this
  PortState port_state = PORT_STATE_UP;

  return port_state;
}

::util::Status NP4ChassisManager::GetPortCounters(uint64 node_id,
                                                  uint32 port_id,
                                                  PortCounters* counters) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ASSIGN_OR_RETURN(auto* singleton, GetSingletonPort(node_id, port_id));
  if (singleton->config_params().admin_state() != ADMIN_STATE_ENABLED) {
    VLOG(1) << "NP4ChassisManager::GetPortCounters : port " << port_id
            << " in node " << node_id << " is not enabled,"
            << " so stats will be set to 0.";
    counters->Clear();
    return ::util::OkStatus();
  }

  return ::util::OkStatus();
}

std::unique_ptr<NP4ChassisManager> NP4ChassisManager::CreateInstance(
    PhalInterface* phal_interface) {
  return absl::WrapUnique(new NP4ChassisManager(phal_interface));
}

void NP4ChassisManager::SendPortOperStateGnmiEvent(uint64 node_id,
                                                   uint32 port_id,
                                                   PortState new_state) {
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

void NP4ChassisManager::ReadPortStatusChangeEvents() {
  PortStatusChangeEvent event;
  while (true) {
    // port_status_change_event_reader_ does not need to be protected by a mutex
    // because this thread is the only one accessing it. It is assigned in
    // RegisterEventWriters and then left untouched until UnregisterEventWriters
    // is called. UnregisterEventWriters joins this thread before resetting the
    // reader.
    int code =
        port_status_change_event_reader_->Read(&event, absl::InfiniteDuration())
            .error_code();
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
      auto* port_id_to_port_state =
          gtl::FindOrNull(node_id_to_port_id_to_port_state_, event.node_id);
      if (port_id_to_port_state == nullptr) {
        LOG(ERROR) << "Node " << event.node_id
                   << " is not configured or not known.";
      }
      auto* port_state_ptr =
          gtl::FindOrNull(*port_id_to_port_state, event.port_id);
      if (port_state_ptr == nullptr) {
        LOG(ERROR) << "Port " << event.port_id
                   << " is not configured or not known for node "
                   << event.node_id << ".";
      }
      LOG(INFO) << "State of port " << event.port_id << " in node "
                << event.node_id << ": " << PrintPortState(event.state) << ".";
      *port_state_ptr = event.state;
      SendPortOperStateGnmiEvent(event.node_id, event.port_id, event.state);
    }
  }
}

::util::Status NP4ChassisManager::RegisterEventWriters() {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "RegisterEventWriters() can be called only before the class is "
           << "initialized.";
  }

  port_status_change_event_channel_ =
      Channel<PortStatusChangeEvent>::Create(kMaxPortStatusChangeEventDepth);

  absl::WriterMutexLock l(&port_status_change_event_writer_lock_);
  port_status_change_event_writer_ =
      ChannelWriter<PortStatusChangeEvent>::Create(
          port_status_change_event_channel_);
  port_status_change_event_reader_ =
      ChannelReader<PortStatusChangeEvent>::Create(
          port_status_change_event_channel_);

  port_status_change_event_thread_ =
      std::thread([this]() { this->ReadPortStatusChangeEvents(); });

  return ::util::OkStatus();
}

::util::Status NP4ChassisManager::UnregisterEventWriters() {
  absl::WriterMutexLock l(&chassis_lock);
  ::util::Status status = ::util::OkStatus();
  if (!port_status_change_event_channel_->Close()) {
    APPEND_ERROR(status)
        << "Error when closing port status change event channel.";
  }
  port_status_change_event_thread_.join();
  // Once the thread is joined, it is safe to reset these pointers.
  port_status_change_event_reader_ = nullptr;
  {
    absl::WriterMutexLock l(&port_status_change_event_writer_lock_);
    port_status_change_event_writer_ = nullptr;
  }
  port_status_change_event_channel_ = nullptr;
  return status;
}

void NP4ChassisManager::CleanupInternalState() {
  node_id_to_port_id_to_port_state_.clear();
  node_id_to_port_id_to_port_config_.clear();
}

::util::Status NP4ChassisManager::Shutdown() {
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
    CleanupInternalState();
  }
  return status;
}

}  // namespace np4intel
}  // namespace hal
}  // namespace stratum
