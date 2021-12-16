// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/numbers.h"
#include "gflags/gflags.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/message.h"
#include "grpcpp/grpcpp.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/hal/lib/dummy/dummy_test.grpc.pb.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

#define RETURN_IF_GRPC_ERROR(expr)                                           \
  do {                                                                       \
    const ::grpc::Status _grpc_status = (expr);                              \
    if (ABSL_PREDICT_FALSE(!_grpc_status.ok() &&                             \
                           _grpc_status.error_code() != grpc::CANCELLED)) {  \
      ::util::Status _status(                                                \
          static_cast<::util::error::Code>(_grpc_status.error_code()),       \
          _grpc_status.error_message());                                     \
      LOG(ERROR) << "Return Error: " << #expr << " failed with " << _status; \
      return _status;                                                        \
    }                                                                        \
  } while (0)

DEFINE_bool(dry_run, false, "Don't change any state");
DEFINE_string(grpc_addr, "localhost:28010", "dummy server gRPC address");

namespace stratum {
namespace hal {
namespace dummy_switch {
namespace {

const char kUsage[] =
    R"USAGE(usage:
)USAGE";

const absl::flat_hash_set<std::string> node_port_states = {
    "oper_status",          "admin_status",          "mac_address",
    "port_speed",           "negotiated_port_speed", "lacp_router_mac",
    "lacp_system_priority", "port_counters",         "forwarding_viability",
    "health_indicator",
};
const absl::flat_hash_set<std::string> node_states = {
    "node_packetio_debug_info"};
const absl::flat_hash_set<std::string> chassis_states = {
    "memory_error_alarm",
    "flow_programming_exception_alarm",
};
const absl::flat_hash_set<std::string> port_queue_states = {
    "port_qos_counters"};

::util::StatusOr<OperStatus> ParseOperStatus(const std::string& arg) {
  PortState s;
  CHECK_RETURN_IF_FALSE(PortState_Parse(arg, &s));
  OperStatus os;
  os.set_state(s);
  return os;
}

::util::StatusOr<AdminStatus> ParseAdminStatus(const std::string& arg) {
  AdminState s;
  CHECK_RETURN_IF_FALSE(AdminState_Parse(arg, &s));
  AdminStatus as;
  as.set_state(s);
  return as;
}

::util::StatusOr<MacAddress> ParseMacAddress(const std::string& arg) {
  CHECK_RETURN_IF_FALSE(IsMacAddressValid(arg));
  MacAddress ma;
  ma.set_mac_address(YangStringToMacAddress(arg));
  return ma;
}

::util::StatusOr<PortSpeed> ParsePortSpeed(const std::string& arg) {
  uint64 speed;
  CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(arg, &speed));
  PortSpeed ps;
  ps.set_speed_bps(speed);
  return ps;
}

::util::StatusOr<PortCounters> ParsePortCounters(
    const std::vector<std::string>& args) {
  PortCounters pc;
  for (int i = 0; i < args.size() && i < pc.GetDescriptor()->field_count();
       ++i) {
    uint64 counter;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[i], &counter));
    pc.GetReflection()->SetUInt64(&pc, pc.GetDescriptor()->field(i), counter);
  }
  return pc;
}

::util::StatusOr<ForwardingViability> ParseForwardingViability(
    const std::string& arg) {
  TrunkMemberBlockState s;
  CHECK_RETURN_IF_FALSE(TrunkMemberBlockState_Parse(arg, &s));
  ForwardingViability fv;
  fv.set_state(s);
  return fv;
}

::util::StatusOr<HealthIndicator> ParseHealthIndicator(const std::string& arg) {
  HealthState s;
  CHECK_RETURN_IF_FALSE(HealthState_Parse(arg, &s));
  HealthIndicator hi;
  hi.set_state(s);
  return hi;
}

bool StringToBool(std::string str) {
  return (str == "y") || (str == "true") || (str == "t") || (str == "yes") ||
         (str == "1");
}

::util::StatusOr<Alarm> ParseAlarm(const std::vector<std::string>& args) {
  Alarm a;
  if (args.size() > 0) {
    uint64 time_created;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[0], &time_created));
    a.set_time_created(time_created);
  }
  if (args.size() > 1) {
    a.set_description(args[1]);
  }
  if (args.size() > 2) {
    Alarm::Severity s;
    CHECK_RETURN_IF_FALSE(Alarm::Severity_Parse(args[2], &s));
    a.set_severity(s);
  }
  if (args.size() > 3) {
    a.set_status(StringToBool(args[3]));
  }
  return a;
}

::util::StatusOr<PortQosCounters> ParsePortQosCounters(
    const std::vector<std::string>& args) {
  PortQosCounters pqc;
  if (args.size() > 0) {
    uint32 queue_id;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[0], &queue_id));
    pqc.set_queue_id(queue_id);
  }
  if (args.size() > 1) {
    uint64 out_octets;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[1], &out_octets));
    pqc.set_out_octets(out_octets);
  }
  if (args.size() > 2) {
    uint64 out_pkts;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[1], &out_pkts));
    pqc.set_out_pkts(out_pkts);
  }
  if (args.size() > 3) {
    uint64 out_dropped_pkts;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[1], &out_dropped_pkts));
    pqc.set_out_dropped_pkts(out_dropped_pkts);
  }

  return pqc;
}

::util::StatusOr<DataResponse> ParsePortNodeStates(
    const std::vector<std::string>& args) {
  DataResponse new_state;
  std::string state = args[0];
  if (state == "oper_status") {
    ASSIGN_OR_RETURN(*new_state.mutable_oper_status(),
                     ParseOperStatus(args[3]));
  } else if (state == "admin_status") {
    ASSIGN_OR_RETURN(*new_state.mutable_admin_status(),
                     ParseAdminStatus(args[3]));
  } else if (state == "mac_address") {
    ASSIGN_OR_RETURN(*new_state.mutable_mac_address(),
                     ParseMacAddress(args[3]));
  } else if (state == "port_speed") {
    ASSIGN_OR_RETURN(*new_state.mutable_port_speed(), ParsePortSpeed(args[3]));
  } else if (state == "negotiated_port_speed") {
    ASSIGN_OR_RETURN(*new_state.mutable_negotiated_port_speed(),
                     ParsePortSpeed(args[3]));
  } else if (state == "lacp_router_mac") {
    ASSIGN_OR_RETURN(*new_state.mutable_lacp_router_mac(),
                     ParseMacAddress(args[3]));
  } else if (state == "port_counters") {
    std::vector<std::string> counter_args(args.begin() + 3, args.end());
    ASSIGN_OR_RETURN(*new_state.mutable_port_counters(),
                     ParsePortCounters(counter_args));
  } else if (state == "forwarding_viability") {
    ASSIGN_OR_RETURN(*new_state.mutable_forwarding_viability(),
                     ParseForwardingViability(args[3]));
  } else if (state == "health_indicator") {
    ASSIGN_OR_RETURN(*new_state.mutable_health_indicator(),
                     ParseHealthIndicator(args[3]));
  } else {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
  }

  return new_state;
}

::util::StatusOr<DataResponse> ParseNodeStates(
    const std::vector<std::string>& args) {
  DataResponse new_state;
  std::string state = args[0];
  if (state == "node_packetio_debug_info") {
    new_state.mutable_node_packetio_debug_info()->set_debug_string(args[2]);
  } else {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
  }

  return new_state;
}

::util::StatusOr<DataResponse> ParseChassisStates(
    const std::vector<std::string>& args) {
  DataResponse new_state;
  std::string state = args[0];
  std::vector<std::string> alarm_args(args.begin() + 1, args.end());
  if (state == "memory_error_alarm") {
    ASSIGN_OR_RETURN(*new_state.mutable_memory_error_alarm(),
                     ParseAlarm(alarm_args));
  } else if (state == "flow_programming_exception_alarm") {
    ASSIGN_OR_RETURN(*new_state.mutable_flow_programming_exception_alarm(),
                     ParseAlarm(alarm_args));
  } else {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
  }

  return new_state;
}

::util::StatusOr<DataResponse> ParsePortQueueNodeStates(
    const std::vector<std::string>& args) {
  DataResponse new_state;
  std::string state = args[0];
  std::vector<std::string> port_queue_args(args.begin() + 2, args.end());
  if (state == "port_qos_counters") {
    ASSIGN_OR_RETURN(*new_state.mutable_port_qos_counters(),
                     ParsePortQosCounters(port_queue_args));
  } else {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
  }

  return new_state;
}

::util::StatusOr<DeviceStatusUpdateRequest> ParseRequest(
    const std::vector<std::string>& args) {
  DeviceStatusUpdateRequest req;

  CHECK_RETURN_IF_FALSE(args.size()) << "Invalid arguments. Missing state.";
  std::string state = args[0];

  if (gtl::ContainsKey(node_port_states, state)) {
    CHECK_RETURN_IF_FALSE(args.size() >= 4)
        << "Invalid number of args. Expected node port value(s).";
    uint64 node;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[1], &node));
    uint32 port;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[2], &port));
    req.mutable_source()->mutable_port()->set_node_id(node);
    req.mutable_source()->mutable_port()->set_port_id(port);
    ASSIGN_OR_RETURN(*req.mutable_state_update(), ParsePortNodeStates(args));
  } else if (gtl::ContainsKey(node_states, state)) {
    CHECK_RETURN_IF_FALSE(args.size() >= 3)
        << "Invalid number of args. Expected node value(s).";
    uint64 node;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[1], &node));
    req.mutable_source()->mutable_node()->set_node_id(node);
    ASSIGN_OR_RETURN(*req.mutable_state_update(), ParseNodeStates(args));
  } else if (gtl::ContainsKey(chassis_states, state)) {
    CHECK_RETURN_IF_FALSE(args.size() >= 2)
        << "Invalid number of args. Expected value(s).";
    req.mutable_source()->mutable_chassis();
    ASSIGN_OR_RETURN(*req.mutable_state_update(), ParseChassisStates(args));
  } else if (gtl::ContainsKey(port_queue_states, state)) {
    CHECK_RETURN_IF_FALSE(args.size() >= 5)
        << "Invalid number of args. Expected node port queue value(s).";
    uint64 node;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[1], &node));
    uint32 port;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[2], &port));
    uint32 queue;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(args[3], &queue));
    req.mutable_source()->mutable_port_queue()->set_node_id(node);
    req.mutable_source()->mutable_port_queue()->set_port_id(port);
    req.mutable_source()->mutable_port_queue()->set_queue_id(queue);
    ASSIGN_OR_RETURN(*req.mutable_state_update(),
                     ParsePortQueueNodeStates(args));
  } else {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
  }

  return req;
}

::util::Status Main(int argc_, char** argv_) {
  ::gflags::SetUsageMessage(kUsage);
  InitGoogle(argv_[0], &argc_, &argv_, true);
  InitStratumLogging();

  std::vector<std::string> argv;
  for (int i = 1 /*skip prog name*/; i < argc_; ++i) {
    argv.push_back(argv_[i]);
  }
  VLOG(1) << PrintVector(argv, ", ");

  std::shared_ptr<::grpc::ChannelCredentials> channel_credentials =
      ::grpc::InsecureChannelCredentials();
  auto channel = ::grpc::CreateChannel(FLAGS_grpc_addr, channel_credentials);
  auto stub = Test::NewStub(channel);

  ASSIGN_OR_RETURN(DeviceStatusUpdateRequest req, ParseRequest(argv));
  LOG(INFO) << req.DebugString();

  if (FLAGS_dry_run) {
    return ::util::OkStatus();
  }

  ::grpc::ClientContext ctx;
  ctx.set_wait_for_ready(false);  // fail fast
  DeviceStatusUpdateResponse resp;
  RETURN_IF_GRPC_ERROR(stub->DeviceStatusUpdate(&ctx, req, &resp));
  LOG(INFO) << resp.DebugString();

  return ::util::OkStatus();
}
}  // namespace
}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::hal::dummy_switch::Main(argc, argv).error_code();
}
