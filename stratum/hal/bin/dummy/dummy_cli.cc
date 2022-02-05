// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/random/random.h"
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

#define RETURN_IF_GRPC_ERROR(expr)                                            \
  do {                                                                        \
    const ::grpc::Status _grpc_status = (expr);                               \
    if (ABSL_PREDICT_FALSE(!_grpc_status.ok() &&                              \
                           _grpc_status.error_code() != ::grpc::CANCELLED)) { \
      ::util::Status _status(                                                 \
          static_cast<::util::error::Code>(_grpc_status.error_code()),        \
          _grpc_status.error_message());                                      \
      LOG(ERROR) << "Return Error: " << #expr << " failed with " << _status;  \
      return _status;                                                         \
    }                                                                         \
  } while (0)

DEFINE_string(grpc_addr, "localhost:28010", "dummy server gRPC address");
DEFINE_bool(port_counter_sim_mode, false,
            "Continuously simulate port counter updates");
DEFINE_uint64(delay_s, 1, "Delay in seconds between port counter updates");
DEFINE_bool(dry_run, false, "Don't change any state");

namespace stratum {
namespace hal {
namespace dummy_switch {
namespace {

class DummyCli {
 public:
  static constexpr char kUsage[] =
      R"USAGE(usage: [--help] [--dry-run] [--grpc_addr TEST_SERVICE_URL]
    {oper_status,admin_status,mac_address,port_speed,negotiated_port_speed,
    lacp_router_mac,lacp_system_priority,port_counters,forwarding_viability,
    health_indicator,node_packetio_debug_info,memory_error_alarm,
    flow_programming_exception_alarm,port_qos_counters}
  )USAGE";

  explicit DummyCli(std::shared_ptr<::grpc::Channel> channel)
      : stub_(Test::NewStub(channel)) {}

  ::util::Status RunCli(const std::vector<std::string>& args) {
    if (FLAGS_port_counter_sim_mode) {
      return DoPortCounterSim(args);
    } else {
      return DoOneShotRequest(args);
    }
  }

 private:
  const absl::flat_hash_set<std::string> kNodePortStates = {
      "oper_status",          "admin_status",          "mac_address",
      "port_speed",           "negotiated_port_speed", "lacp_router_mac",
      "lacp_system_priority", "port_counters",         "forwarding_viability",
      "health_indicator",
  };
  const absl::flat_hash_set<std::string> kNodeStates = {
      "node_packetio_debug_info"};
  const absl::flat_hash_set<std::string> kChassisStates = {
      "memory_error_alarm",
      "flow_programming_exception_alarm",
  };
  const absl::flat_hash_set<std::string> kPortQueueStates = {
      "port_qos_counters"};

  ::util::StatusOr<OperStatus> ParseOperStatus(const std::string& arg) {
    PortState s;
    RET_CHECK(PortState_Parse(arg, &s));
    OperStatus os;
    os.set_state(s);
    return os;
  }

  ::util::StatusOr<AdminStatus> ParseAdminStatus(const std::string& arg) {
    AdminState s;
    RET_CHECK(AdminState_Parse(arg, &s));
    AdminStatus as;
    as.set_state(s);
    return as;
  }

  ::util::StatusOr<MacAddress> ParseMacAddress(const std::string& arg) {
    ASSIGN_OR_RETURN(uint64 mac_address, YangStringToMacAddress(arg));
    MacAddress ma;
    ma.set_mac_address(mac_address);
    return ma;
  }

  ::util::StatusOr<PortSpeed> ParsePortSpeed(const std::string& arg) {
    uint64 speed;
    RET_CHECK(absl::SimpleAtoi(arg, &speed));
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
      RET_CHECK(absl::SimpleAtoi(args[i], &counter));
      pc.GetReflection()->SetUInt64(&pc, pc.GetDescriptor()->field(i), counter);
    }
    return pc;
  }

  ::util::StatusOr<ForwardingViability> ParseForwardingViability(
      const std::string& arg) {
    TrunkMemberBlockState s;
    RET_CHECK(TrunkMemberBlockState_Parse(arg, &s));
    ForwardingViability fv;
    fv.set_state(s);
    return fv;
  }

  ::util::StatusOr<HealthIndicator> ParseHealthIndicator(
      const std::string& arg) {
    HealthState s;
    RET_CHECK(HealthState_Parse(arg, &s));
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
      RET_CHECK(absl::SimpleAtoi(args[0], &time_created));
      a.set_time_created(time_created);
    }
    if (args.size() > 1) {
      a.set_description(args[1]);
    }
    if (args.size() > 2) {
      Alarm::Severity s;
      RET_CHECK(Alarm::Severity_Parse(args[2], &s));
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
      RET_CHECK(absl::SimpleAtoi(args[0], &queue_id));
      pqc.set_queue_id(queue_id);
    }
    if (args.size() > 1) {
      uint64 out_octets;
      RET_CHECK(absl::SimpleAtoi(args[1], &out_octets));
      pqc.set_out_octets(out_octets);
    }
    if (args.size() > 2) {
      uint64 out_pkts;
      RET_CHECK(absl::SimpleAtoi(args[1], &out_pkts));
      pqc.set_out_pkts(out_pkts);
    }
    if (args.size() > 3) {
      uint64 out_dropped_pkts;
      RET_CHECK(absl::SimpleAtoi(args[1], &out_dropped_pkts));
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
      ASSIGN_OR_RETURN(*new_state.mutable_port_speed(),
                       ParsePortSpeed(args[3]));
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
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
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
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
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
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
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
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
    }

    return new_state;
  }

  ::util::StatusOr<DeviceStatusUpdateRequest> ParseRequest(
      const std::vector<std::string>& args) {
    DeviceStatusUpdateRequest req;

    RET_CHECK(args.size()) << "Invalid arguments. Missing state.";
    std::string state = args[0];

    if (gtl::ContainsKey(kNodePortStates, state)) {
      RET_CHECK(args.size() >= 4)
          << "Invalid number of args. Expected node port value(s).";
      uint64 node;
      RET_CHECK(absl::SimpleAtoi(args[1], &node));
      uint32 port;
      RET_CHECK(absl::SimpleAtoi(args[2], &port));
      req.mutable_source()->mutable_port()->set_node_id(node);
      req.mutable_source()->mutable_port()->set_port_id(port);
      ASSIGN_OR_RETURN(*req.mutable_state_update(), ParsePortNodeStates(args));
    } else if (gtl::ContainsKey(kNodeStates, state)) {
      RET_CHECK(args.size() >= 3)
          << "Invalid number of args. Expected node value(s).";
      uint64 node;
      RET_CHECK(absl::SimpleAtoi(args[1], &node));
      req.mutable_source()->mutable_node()->set_node_id(node);
      ASSIGN_OR_RETURN(*req.mutable_state_update(), ParseNodeStates(args));
    } else if (gtl::ContainsKey(kChassisStates, state)) {
      RET_CHECK(args.size() >= 2)
          << "Invalid number of args. Expected value(s).";
      req.mutable_source()->mutable_chassis();
      ASSIGN_OR_RETURN(*req.mutable_state_update(), ParseChassisStates(args));
    } else if (gtl::ContainsKey(kPortQueueStates, state)) {
      RET_CHECK(args.size() >= 5)
          << "Invalid number of args. Expected node port queue value(s).";
      uint64 node;
      RET_CHECK(absl::SimpleAtoi(args[1], &node));
      uint32 port;
      RET_CHECK(absl::SimpleAtoi(args[2], &port));
      uint32 queue;
      RET_CHECK(absl::SimpleAtoi(args[3], &queue));
      req.mutable_source()->mutable_port_queue()->set_node_id(node);
      req.mutable_source()->mutable_port_queue()->set_port_id(port);
      req.mutable_source()->mutable_port_queue()->set_queue_id(queue);
      ASSIGN_OR_RETURN(*req.mutable_state_update(),
                       ParsePortQueueNodeStates(args));
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid state " << state << ".";
    }

    return req;
  }

  ::util::Status DoOneShotRequest(const std::vector<std::string>& args) {
    ASSIGN_OR_RETURN(DeviceStatusUpdateRequest req, ParseRequest(args));
    LOG(INFO) << req.DebugString();

    ::grpc::ClientContext ctx;
    ctx.set_wait_for_ready(false);  // fail fast
    DeviceStatusUpdateResponse resp;
    if (FLAGS_dry_run) {
      return ::util::OkStatus();
    }
    RETURN_IF_GRPC_ERROR(stub_->DeviceStatusUpdate(&ctx, req, &resp));
    LOG(INFO) << resp.DebugString();

    return ::util::OkStatus();
  }

  ::util::Status DoPortCounterSim(const std::vector<std::string>& args) {
    RET_CHECK(args.size() == 2)
        << "Invalid number of args. Expected node port.";
    uint64 node;
    RET_CHECK(absl::SimpleAtoi(args[0], &node));
    uint32 port;
    RET_CHECK(absl::SimpleAtoi(args[1], &port));
    DeviceStatusUpdateRequest req;
    req.mutable_source()->mutable_port()->set_node_id(node);
    req.mutable_source()->mutable_port()->set_port_id(port);
    ASSIGN_OR_RETURN(*req.mutable_state_update()->mutable_port_counters(),
                     ParsePortCounters({}));

    absl::BitGen bitgen;
    while (true) {
      req.mutable_state_update()->mutable_port_counters()->set_in_octets(
          req.state_update().port_counters().in_octets() +
          absl::Uniform(bitgen, 1u, 10u) * kBitsPerMegabit);
      req.mutable_state_update()->mutable_port_counters()->set_in_unicast_pkts(
          req.state_update().port_counters().in_unicast_pkts() +
          absl::Uniform(bitgen, 1u, 10u));
      req.mutable_state_update()->mutable_port_counters()->set_out_octets(
          req.state_update().port_counters().out_octets() +
          absl::Uniform(bitgen, 1u, 10u) * kBitsPerMegabit);
      req.mutable_state_update()->mutable_port_counters()->set_out_unicast_pkts(
          req.state_update().port_counters().out_unicast_pkts() +
          absl::Uniform(bitgen, 1u, 10u));
      LOG(INFO) << req.DebugString();
      ::grpc::ClientContext ctx;
      ctx.set_wait_for_ready(false);  // fail fast
      DeviceStatusUpdateResponse resp;
      if (!FLAGS_dry_run) {
        RETURN_IF_GRPC_ERROR(stub_->DeviceStatusUpdate(&ctx, req, &resp));
      }

      absl::SleepFor(absl::Seconds(FLAGS_delay_s));
    }
  }

  std::unique_ptr<Test::Stub> stub_;
};

constexpr char DummyCli::kUsage[];

::util::Status Main(int argc_, char** argv_) {
  ::gflags::SetUsageMessage(DummyCli::kUsage);
  InitGoogle(argv_[0], &argc_, &argv_, true);
  InitStratumLogging();

  std::vector<std::string> argv;
  for (int i = 1 /*skip prog name*/; i < argc_; ++i) {
    argv.push_back(argv_[i]);
  }

  DummyCli cli(::grpc::CreateChannel(FLAGS_grpc_addr,
                                     ::grpc::InsecureChannelCredentials()));
  return cli.RunCli(argv);
}
}  // namespace
}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::hal::dummy_switch::Main(argc, argv).error_code();
}
