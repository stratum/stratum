// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bmv2/bmv2_chassis_manager.h"

#include <functional>  // std::bind
#include <map>
#include <memory>
#include <utility>  // std::pair

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "bm/bm_sim/dev_mgr.h"
#include "bm/simple_switch/runner.h"
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
namespace bmv2 {

constexpr int Bmv2ChassisManager::kMaxPortStatusChangeEventDepth;

absl::Mutex chassis_lock;

Bmv2ChassisManager::Bmv2ChassisManager(
    PhalInterface* phal_interface,
    std::map<uint64, ::bm::sswitch::SimpleSwitchRunner*> node_id_to_bmv2_runner)
    : initialized_(false),
      phal_interface_(phal_interface),
      port_status_change_event_channel_(nullptr),
      port_status_change_event_reader_(nullptr),
      port_status_change_event_writer_(nullptr),
      node_id_to_bmv2_runner_(node_id_to_bmv2_runner),
      node_id_to_bmv2_port_status_cb_(),
      node_id_to_port_id_to_port_state_(),
      node_id_to_port_id_to_port_config_() {
  for (auto& node : node_id_to_bmv2_runner) {
    CHECK_EQ(node.first, node.second->get_device_id())
        << "Device / node id mismatch with bmv2 runner";
    node_id_to_port_id_to_port_state_[node.first] = {};
    node_id_to_port_id_to_port_config_[node.first] = {};
  }
}

Bmv2ChassisManager::~Bmv2ChassisManager() = default;

namespace {

// helper to add a bmv2 port
::util::Status AddPort(::bm::DevMgr* dev_mgr, uint64 node_id,
                       const std::string& port_name, uint32 port_id) {
  LOG(INFO) << "Adding port " << port_id << " to node " << node_id;
  // port_name can be "<interface_name>"
  // or "<arbitrary_string>@<interface_name>"
  auto start_iface_name = port_name.find_last_of("@");
  auto iface_name = (start_iface_name == std::string::npos)
                        ? port_name
                        : port_name.substr(start_iface_name + 1);
  auto bm_status = dev_mgr->port_add(
      iface_name, static_cast<bm::PortMonitorIface::port_t>(port_id), {});
  if (bm_status != bm::DevMgrIface::ReturnCode::SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Error when binding port " << port_id << " to interface "
           << iface_name << " in node " << node_id << ".";
  }
  return ::util::OkStatus();
}

// helper to remove a bmv2 port
::util::Status RemovePort(::bm::DevMgr* dev_mgr, uint64 node_id,
                          uint32 port_id) {
  LOG(INFO) << "Removing port " << port_id << " from node " << node_id;
  auto bm_status =
      dev_mgr->port_remove(static_cast<bm::PortMonitorIface::port_t>(port_id));
  if (bm_status != bm::DevMgrIface::ReturnCode::SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error when removing port " << port_id
                                    << " from node " << node_id << ".";
  }
  return ::util::OkStatus();
}

}  // namespace

::util::Status Bmv2ChassisManager::PushChassisConfig(
    const ChassisConfig& config) {
  VLOG(1) << "Bmv2ChassisManager::PushChassisConfig";
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
    CHECK_RETURN_IF_FALSE(node_id_to_bmv2_runner_.count(node_id) > 0)
        << "Node " << node_id << " is not known.";
    node_id_to_port_id_to_port_state[node_id][port_id] = PORT_STATE_UNKNOWN;
    node_id_to_port_id_to_port_config[node_id][port_id] = singleton_port;
  }

  CHECK_RETURN_IF_FALSE(static_cast<size_t>(config.nodes_size()) ==
                        node_id_to_bmv2_runner_.size())
      << "Missing nodes in ChassisConfig";

  // Compare ports in old config and new config and perform the necessary
  // operations.
  for (auto& node : config.nodes()) {
    VLOG(1) << "Updating config for node " << node.id() << ".";

    auto* runner = gtl::FindOrNull(node_id_to_bmv2_runner_, node.id());
    CHECK_RETURN_IF_FALSE(runner != nullptr)
        << "Cannot find runner for node " << node.id() << ".";
    auto dev_mgr = (*runner)->get_dev_mgr();

    for (const auto& port_old : node_id_to_port_id_to_port_config_[node.id()]) {
      auto port_id = port_old.first;
      auto* singleton_port = gtl::FindOrNull(
          node_id_to_port_id_to_port_config[node.id()], port_id);

      if (singleton_port == nullptr) {  // remove port if not present any more
        auto& config_old = port_old.second.config_params();
        if (config_old.admin_state() == ADMIN_STATE_ENABLED) {
          APPEND_STATUS_IF_ERROR(status,
                                 RemovePort(dev_mgr, node.id(), port_id));
        }
      } else {  // change port config if needed
        auto& config_old = port_old.second.config_params();
        auto& config = singleton_port->config_params();
        if (config.admin_state() != config_old.admin_state()) {
          if (config.admin_state() == ADMIN_STATE_ENABLED) {
            APPEND_STATUS_IF_ERROR(
                status,
                AddPort(dev_mgr, node.id(), singleton_port->name(), port_id));
          } else {
            APPEND_STATUS_IF_ERROR(status,
                                   RemovePort(dev_mgr, node.id(), port_id));
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

    for (const auto& port : node_id_to_port_id_to_port_config[node.id()]) {
      auto port_id = port.first;
      auto* singleton_port_old = gtl::FindOrNull(
          node_id_to_port_id_to_port_config_[node.id()], port_id);

      if (singleton_port_old == nullptr) {  // add new port
        auto& singleton_port = port.second;
        auto& config = singleton_port.config_params();
        if (config.admin_state() == ADMIN_STATE_ENABLED) {
          APPEND_STATUS_IF_ERROR(
              status,
              AddPort(dev_mgr, node.id(), singleton_port.name(), port_id));
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

::util::StatusOr<const SingletonPort*> Bmv2ChassisManager::GetSingletonPort(
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

::util::StatusOr<DataResponse> Bmv2ChassisManager::GetPortData(
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
    case Request::kMacAddress: {
      // TODO(unknown) Find out why the controller needs it.
      // Find MAC address of port located at:
      // - node_id: req.mac_address().node_id()
      // - port_id: req.mac_address().port_id()
      // and then write it into the response.
      resp.mutable_mac_address()->set_mac_address(kDummyMacAddress);
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
    case DataRequest::Request::kLacpRouterMac: {
      // Find LACP System ID MAC address of port located at:
      // - node_id: req.lacp_router_mac().node_id()
      // - port_id: req.lacp_router_mac().port_id()
      // and then write it into the response.
      resp.mutable_lacp_router_mac()->set_mac_address(kDummyMacAddress);
      break;
    }
    case Request::kPortCounters: {
      RETURN_IF_ERROR(GetPortCounters(request.port_counters().node_id(),
                                      request.port_counters().port_id(),
                                      resp.mutable_port_counters()));
      break;
    }
    case DataRequest::Request::kForwardingViability: {
      // Find current port forwarding viable state for port located at:
      // - node_id: req.forwarding_viable().node_id()
      // - port_id: req.forwarding_viable().port_id()
      // and then write it into the response.
      resp.mutable_forwarding_viability()->set_state(
          TRUNK_MEMBER_BLOCK_STATE_UNKNOWN);
      break;
    }
    case DataRequest::Request::kHealthIndicator: {
      // Find current port health indicator (LED) for port located at:
      // - node_id: req.health_indicator().node_id()
      // - port_id: req.health_indicator().port_id()
      // and then write it into the response.
      resp.mutable_health_indicator()->set_state(HEALTH_STATE_UNKNOWN);
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
    case DataRequest::Request::kSdnPortId: {
      resp.mutable_sdn_port_id()->set_port_id(request.sdn_port_id().port_id());
      break;
    }
    default:
      return MAKE_ERROR(ERR_INTERNAL) << "Not supported yet";
  }
  return resp;
}

::util::StatusOr<PortState> Bmv2ChassisManager::GetPortState(uint64 node_id,
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
            << " with bmv2";
  // should not be NULL because we already validated node_id by looking it up in
  // node_id_to_port_id_to_port_state_
  auto runner = gtl::FindPtrOrNull(node_id_to_bmv2_runner_, node_id);
  if (runner == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "No bmv2 runner for node id " << node_id << ".";
  }
  auto dev_mgr = runner->get_dev_mgr();
  auto is_up =
      dev_mgr->port_is_up(static_cast<bm::PortMonitorIface::port_t>(port_id));
  PortState port_state = is_up ? PORT_STATE_UP : PORT_STATE_DOWN;
  LOG(INFO) << "State of port " << port_id << " in node " << node_id << ": "
            << PrintPortState(port_state);
  return port_state;
}

::util::Status Bmv2ChassisManager::GetPortCounters(uint64 node_id,
                                                   uint32 port_id,
                                                   PortCounters* counters) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ASSIGN_OR_RETURN(auto* singleton, GetSingletonPort(node_id, port_id));
  if (singleton->config_params().admin_state() != ADMIN_STATE_ENABLED) {
    VLOG(1) << "Bmv2ChassisManager::GetPortCounters : port " << port_id
            << " in node " << node_id << " is not enabled,"
            << " so stats will be set to 0.";
    counters->Clear();
    return ::util::OkStatus();
  }
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
  return absl::WrapUnique(
      new Bmv2ChassisManager(phal_interface, node_id_to_bmv2_runner));
}

void Bmv2ChassisManager::SendPortOperStateGnmiEvent(uint64 node_id,
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
          new PortOperStateChangedEvent(node_id, port_id, new_state, 0)))) {
    // Remove WriterInterface if it is no longer operational.
    gnmi_event_writer_.reset();
  }
}

void Bmv2ChassisManager::ReadPortStatusChangeEvents() {
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

::util::Status Bmv2ChassisManager::RegisterEventWriters() {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "RegisterEventWriters() can be called only before the class is "
           << "initialized.";
  }

  port_status_change_event_channel_ =
      Channel<PortStatusChangeEvent>::Create(kMaxPortStatusChangeEventDepth);

  port_status_change_event_writer_ =
      ChannelWriter<PortStatusChangeEvent>::Create(
          port_status_change_event_channel_);
  port_status_change_event_reader_ =
      ChannelReader<PortStatusChangeEvent>::Create(
          port_status_change_event_channel_);

  port_status_change_event_thread_ =
      std::thread([this]() { this->ReadPortStatusChangeEvents(); });

  // Register port status change callbacks with bmv2 runners. In practice, this
  // code is only executed once during the lifetime of the instance, when
  // RegisterEventWriters is called for the first time.
  using PortStatus = bm::DevMgr::PortStatus;
  for (auto& node : node_id_to_bmv2_runner_) {
    auto runner = node.second;
    auto dev_mgr = runner->get_dev_mgr();
    uint64 node_id = node.first;

    if (node_id_to_bmv2_port_status_cb_.count(node_id) > 0) continue;

    auto cb = [this, node_id](bm::PortMonitorIface::port_t port,
                              bm::PortMonitorIface::PortStatus status) {
      absl::ReaderMutexLock l(&this->port_status_change_event_writer_lock_);
      auto* writer = this->port_status_change_event_writer_.get();
      if (writer == nullptr) return;
      auto port_id = static_cast<uint32>(port);
      PortState state;
      if (status == bm::PortMonitorIface::PortStatus::PORT_UP) {
        state = PORT_STATE_UP;
      } else if (status == bm::PortMonitorIface::PortStatus::PORT_DOWN) {
        state = PORT_STATE_DOWN;
      } else {
        LOG(ERROR) << "Invalid port state CB from bmv2 for node " << node_id
                   << ".";
        return;
      }
      writer->Write(PortStatusChangeEvent{node_id, port_id, state},
                    absl::InfiniteDuration());
    };

    auto p = node_id_to_bmv2_port_status_cb_.emplace(node.first, cb);
    auto success = p.second;
    CHECK(success) << "Port status CB already registered for node " << node_id
                   << ".";
    auto& cb_ref = p.first->second;
    dev_mgr->register_status_cb(PortStatus::PORT_UP, cb_ref);
    dev_mgr->register_status_cb(PortStatus::PORT_DOWN, cb_ref);
    LOG(INFO) << "Registered port status callbacks successfully for node "
              << node_id << ".";
  }
  return ::util::OkStatus();
}

::util::Status Bmv2ChassisManager::UnregisterEventWriters() {
  ::util::Status status = ::util::OkStatus();
  if (!port_status_change_event_channel_->Close()) {
    ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                           << "Error when closing port status change"
                           << " event channel.";
    APPEND_STATUS_IF_ERROR(status, error);
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

void Bmv2ChassisManager::CleanupInternalState() {
  node_id_to_port_id_to_port_state_.clear();
  node_id_to_port_id_to_port_config_.clear();
}

::util::Status Bmv2ChassisManager::Shutdown() {
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

}  // namespace bmv2
}  // namespace hal
}  // namespace stratum
