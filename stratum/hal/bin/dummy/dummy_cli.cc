// Copyright 2020-present Open Networking Foundation
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

#include <cstdlib>
#include <string>

#include "grpcpp/grpcpp.h"

#define STRIP_FLAG_HELP 1  // remove additional flag help text from gflag
#include "absl/container/flat_hash_set.h"
#include "gflags/gflags.h"
#include "stratum/glue/integral_types.h"
#include "stratum/hal/lib/dummy/dummy_test.grpc.pb.h"

DECLARE_bool(help);
DEFINE_bool(dry_run, false, "Dry run");
DEFINE_string(test_service_url, "127.0.0.1:28010", "Dummy switch address");

const char kUsage[] =
R"USAGE(Usage: dummy_cli [-h] [--dry-run] [--test-service-url TEST_SERVICE_URL]
{oper_status,admin_status,mac_address,port_speed,negotiated_port_speed,
lacp_router_mac,lacp_system_priority,port_counters,forwarding_viability,
health_indicator,node_packetio_debug_info,memory_error_alarm,
flow_programming_exception_alarm,port_qos_counters}
...)USAGE";

namespace stratum {
namespace hal {
namespace dummy {

const absl::flat_hash_set<std::string> true_strings = {"true", "yes", "t", "y",
                                                       "1"};
bool strToBool(std::string str) {
  return true_strings.find(str) != true_strings.end();
}

namespace {
template <typename T, typename V>
DeviceStatusUpdateRequest NodePortStateUpdate(
    uint64 node_id, uint32 port_id, T* (DataResponse::*mutable_state_func)(),
    void (T::*set_value_func)(V), bool (*enum_parser)(const std::string&, V*),
    std::string str_val) {
  DeviceStatusUpdateRequest request;
  request.mutable_source()->mutable_port()->set_node_id(node_id);
  request.mutable_source()->mutable_port()->set_port_id(port_id);

  T* mutable_state = (request.mutable_state_update()->*mutable_state_func)();
  V val;
  if (!enum_parser(str_val, &val)) {
    std::cout << "Unable to parse value " << str_val << std::endl;
  } else {
    (mutable_state->*set_value_func)(val);
  }
  return request;
}

template <typename T, typename V>
DeviceStatusUpdateRequest NodePortStateUpdate(
    uint64 node_id, uint32 port_id, T* (DataResponse::*mutable_state_func)(),
    void (T::*set_value_func)(V), V val) {
  DeviceStatusUpdateRequest request;
  request.mutable_source()->mutable_port()->set_node_id(node_id);
  request.mutable_source()->mutable_port()->set_port_id(port_id);

  T* mutable_state = (request.mutable_state_update()->*mutable_state_func)();
  (mutable_state->*set_value_func)(val);
  return request;
}

DeviceStatusUpdateRequest ChassisAlarmStateUpdate(
    Alarm* (DataResponse::*muttable_state_func)(), char** argv) {
  DeviceStatusUpdateRequest request;
  request.mutable_source()->mutable_chassis()->ParseFromString("");
  Alarm* alarm = (request.mutable_state_update()->*muttable_state_func)();

  alarm->set_time_created(strtoul(argv[2], nullptr, 10));
  alarm->set_description(argv[3]);
  Alarm::Severity severity;
  if (!Alarm::Severity_Parse(argv[4], &severity)) {
    std::cout << "Unable to parse value " << argv[4] << std::endl;
  } else {
    alarm->set_severity(severity);
  }
  alarm->set_status(strToBool(argv[5]));
  return request;
}

DeviceStatusUpdateRequest NodePortCountersStateUpdate(uint64 node_id,
                                                      uint32 port_id,
                                                      char** argv) {
  DeviceStatusUpdateRequest request;
  request.mutable_source()->mutable_port()->set_node_id(node_id);
  request.mutable_source()->mutable_port()->set_port_id(port_id);

  PortCounters* port_counters =
      request.mutable_state_update()->mutable_port_counters();
  port_counters->set_in_octets(strtoull(argv[4], nullptr, 10));
  port_counters->set_in_unicast_pkts(strtoull(argv[5], nullptr, 10));
  port_counters->set_in_broadcast_pkts(strtoull(argv[6], nullptr, 10));
  port_counters->set_in_multicast_pkts(strtoull(argv[7], nullptr, 10));
  port_counters->set_in_discards(strtoull(argv[8], nullptr, 10));
  port_counters->set_in_errors(strtoull(argv[9], nullptr, 10));
  port_counters->set_in_unknown_protos(strtoull(argv[10], nullptr, 10));
  port_counters->set_out_octets(strtoull(argv[11], nullptr, 10));
  port_counters->set_out_unicast_pkts(strtoull(argv[12], nullptr, 10));
  port_counters->set_out_broadcast_pkts(strtoull(argv[13], nullptr, 10));
  port_counters->set_out_multicast_pkts(strtoull(argv[14], nullptr, 10));
  port_counters->set_out_discards(strtoull(argv[15], nullptr, 10));
  port_counters->set_out_errors(strtoull(argv[16], nullptr, 10));
  port_counters->set_in_fcs_errors(strtoull(argv[17], nullptr, 10));

  return request;
}

DeviceStatusUpdateRequest PortQosCountersStateUpdate(uint64 node_id,
                                                     uint32 port_id,
                                                     uint32 queue_id,
                                                     char** argv) {
  DeviceStatusUpdateRequest request;
  request.mutable_source()->mutable_port_queue()->set_node_id(node_id);
  request.mutable_source()->mutable_port_queue()->set_port_id(port_id);
  request.mutable_source()->mutable_port_queue()->set_queue_id(queue_id);

  PortQosCounters* port_qos_counters =
      request.mutable_state_update()->mutable_port_qos_counters();
  port_qos_counters->set_queue_id(strtoul(argv[5], nullptr, 10));
  port_qos_counters->set_out_octets(strtoull(argv[6], nullptr, 10));
  port_qos_counters->set_out_pkts(strtoull(argv[7], nullptr, 10));
  port_qos_counters->set_out_dropped_pkts(strtoull(argv[8], nullptr, 10));

  return request;
}

const absl::flat_hash_set<std::string> node_port_states = {
    "oper_status",          "admin_status",          "mac_address",
    "port_speed",           "negotiated_port_speed", "lacp_router_mac",
    "lacp_system_priority", "port_counters",         "forwarding_viability",
    "health_indicator"};

const absl::flat_hash_set<std::string> node_states = {
    "node_packetio_debug_info"};

const absl::flat_hash_set<std::string> chassis_states = {
    "memory_error_alarm", "flow_programming_exception_alarm"};

const absl::flat_hash_set<std::string> port_queue_states = {
    "port_qos_counters"};

}  // namespace

int Main(int argc, char** argv) {
  ::grpc::ClientContext ctx;
  ::grpc::Status status;
  auto channel = ::grpc::CreateChannel(FLAGS_test_service_url,
                                       ::grpc::InsecureChannelCredentials());
  auto stub = ::stratum::hal::dummy::Test::NewStub(channel);
  std::string state_to_be_updated = argv[1];
  DeviceStatusUpdateRequest req;

  uint64 node_id;
  uint32 port_id;
  uint32 queue_id;

  if (node_port_states.find(state_to_be_updated) != node_port_states.end()) {
    sscanf(argv[2], "%lu", &node_id);
    sscanf(argv[3], "%u", &port_id);
  }

  if (node_states.find(state_to_be_updated) != node_states.end()) {
    sscanf(argv[2], "%lu", &node_id);
  }

  if (port_queue_states.find(state_to_be_updated) != port_queue_states.end()) {
    sscanf(argv[2], "%lu", &node_id);
    sscanf(argv[3], "%u", &port_id);
    sscanf(argv[4], "%u", &queue_id);
  }

  if (state_to_be_updated.compare("oper_status") == 0) {
    req = NodePortStateUpdate(
        node_id, port_id, &DataResponse::mutable_oper_status,
        &OperStatus::set_state, &PortState_Parse, argv[4]);
  } else if (state_to_be_updated.compare("admin_status") == 0) {
    req = NodePortStateUpdate(
        node_id, port_id, &DataResponse::mutable_admin_status,
        &AdminStatus::set_state, &AdminState_Parse, argv[4]);
  } else if (state_to_be_updated.compare("mac_address") == 0) {
    req = NodePortStateUpdate(
        node_id, port_id, &DataResponse::mutable_mac_address,
        &MacAddress::set_mac_address, strtoul(argv[4], nullptr, 10));
  } else if (state_to_be_updated.compare("port_speed") == 0) {
    req = NodePortStateUpdate(
        node_id, port_id, &DataResponse::mutable_port_speed,
        &PortSpeed::set_speed_bps, strtoul(argv[4], nullptr, 10));
  } else if (state_to_be_updated.compare("negotiated_port_speed") == 0) {
    req = NodePortStateUpdate(
        node_id, port_id, &DataResponse::mutable_negotiated_port_speed,
        &PortSpeed::set_speed_bps, strtoul(argv[4], nullptr, 10));
  } else if (state_to_be_updated.compare("lacp_router_mac") == 0) {
    req = NodePortStateUpdate(
        node_id, port_id, &DataResponse::mutable_lacp_router_mac,
        &MacAddress::set_mac_address, strtoul(argv[4], nullptr, 10));
  } else if (state_to_be_updated.compare("lacp_system_priority") == 0) {
    req = NodePortStateUpdate(node_id, port_id,
                              &DataResponse::mutable_lacp_system_priority,
                              &SystemPriority::set_priority,
                              (unsigned int)strtoul(argv[4], nullptr, 10));
  } else if (state_to_be_updated.compare("forwarding_viability") == 0) {
    req = NodePortStateUpdate(
        node_id, port_id, &DataResponse::mutable_forwarding_viability,
        &ForwardingViability::set_state, &TrunkMemberBlockState_Parse, argv[4]);
  } else if (state_to_be_updated.compare("health_indicator") == 0) {
    req = NodePortStateUpdate(
        node_id, port_id, &DataResponse::mutable_health_indicator,
        &HealthIndicator::set_state, &HealthState_Parse, argv[4]);
  } else if (state_to_be_updated.compare("port_counters") == 0) {
    req = NodePortCountersStateUpdate(node_id, port_id, argv);
  } else if (state_to_be_updated.compare("node_packetio_debug_info") == 0) {
    req.mutable_source()->mutable_node()->set_node_id(node_id);
    NodeDebugInfo* debug_info =
        req.mutable_state_update()->mutable_node_packetio_debug_info();
    debug_info->set_debug_string(argv[4]);
  } else if (state_to_be_updated.compare("memory_error_alarm") == 0) {
    req = ChassisAlarmStateUpdate(&DataResponse::mutable_memory_error_alarm,
                                  argv);
  } else if (state_to_be_updated.compare("flow_programming_exception_alarm") ==
             0) {
    req = ChassisAlarmStateUpdate(
        &DataResponse::mutable_flow_programming_exception_alarm, argv);
  } else if (state_to_be_updated.compare("port_qos_counters") == 0) {
    req = PortQosCountersStateUpdate(node_id, port_id, queue_id, argv);
  } else {
    std::cout << "Unknown request " << state_to_be_updated << std::endl;
  }

  std::cout << "Request: " << std::endl;
  std::cout << req.DebugString() << std::endl;
  if (!FLAGS_dry_run) {
    DeviceStatusUpdateResponse resp;
    status = stub->DeviceStatusUpdate(&ctx, req, &resp);

    if (!status.ok()) {
      std::cout << status.error_message() << std::endl;
    } else {
      std::cout << "Response: " << std::endl;
      std::cout << resp.DebugString() << std::endl;
    }
  }

  return 0;
}

}  // namespace dummy
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  ::gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);

  if (FLAGS_help) {
    std::cout << kUsage << std::endl;
    return 0;
  }
  return stratum::hal::dummy::Main(argc, argv);
}
