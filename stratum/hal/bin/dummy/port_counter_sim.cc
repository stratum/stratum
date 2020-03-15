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
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gflags/gflags.h"
#include "stratum/glue/integral_types.h"
#include "stratum/hal/lib/dummy/dummy_test.grpc.pb.h"

DECLARE_bool(help);
DEFINE_bool(dry_run, false, "Dry run");
DEFINE_string(test_service_url, "127.0.0.1:28010", "Dummy switch address");
DEFINE_uint32(delay, 5000, "Delay between each counter event(ms)");

namespace stratum {
namespace hal {
namespace dummy {

namespace {
bool running = true;
void HandleSignal(int sig) {
  std::cout << "Stopping....." << std::endl;
  running=false;
}
}  // namespace


int Main(int argc, char** argv) {

  signal(SIGINT, HandleSignal);

  uint64 node_id = strtoull(argv[1], nullptr, 10);
  uint32 port_id = strtoul(argv[2], nullptr, 10);
  ::grpc::ClientContext ctx;
  ::grpc::Status status;
  auto channel = ::grpc::CreateChannel(FLAGS_test_service_url,
                                       ::grpc::InsecureChannelCredentials());
  auto stub = ::stratum::hal::dummy::Test::NewStub(channel);
  std::string state_to_be_updated = argv[1];
  uint64 counters[14] = {0};

  while(running) {
    // Update counters
    for (int i = 0; i < 14; i++) {
      counters[i] += 1000;  // +1000 packets/bytes
    }

    DeviceStatusUpdateRequest request;
    DeviceStatusUpdateResponse response;
    request.mutable_source()->mutable_port()->set_node_id(node_id);
    request.mutable_source()->mutable_port()->set_port_id(port_id);

    PortCounters* port_counters =
        request.mutable_state_update()->mutable_port_counters();
    port_counters->set_in_octets(counters[0]);
    port_counters->set_in_unicast_pkts(counters[1]);
    port_counters->set_in_broadcast_pkts(counters[2]);
    port_counters->set_in_multicast_pkts(counters[3]);
    port_counters->set_in_discards(counters[4]);
    port_counters->set_in_errors(counters[5]);
    port_counters->set_in_unknown_protos(counters[6]);
    port_counters->set_out_octets(counters[7]);
    port_counters->set_out_unicast_pkts(counters[8]);
    port_counters->set_out_broadcast_pkts(counters[9]);
    port_counters->set_out_multicast_pkts(counters[10]);
    port_counters->set_out_discards(counters[11]);
    port_counters->set_out_errors(counters[12]);
    port_counters->set_in_fcs_errors(counters[13]);

    std::cout << "Request: " << std::endl;
    std::cout << request.DebugString() << std::endl;
    if (!FLAGS_dry_run) {
      status = stub->DeviceStatusUpdate(&ctx, request, &response);
      if (!status.ok()) {
        std::cout << status.error_message() << std::endl;
      } else {
        std::cout << "Response: " << std::endl;
        std::cout << response.DebugString() << std::endl;
      }
    }
    absl::SleepFor(absl::Milliseconds(FLAGS_delay));
  }

  return 0;
}

}  // namespace stratum
}  // namespace hal
}  // namespace dummy


const char kUsage[] =
R"U(Usage: port_counter_sim [--dry-run] [--test-service-url TEST_SERVICE_URL]
[--delay DELAY] node_id port_id
)U";

int main(int argc, char** argv) {
  ::gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
  if (argc < 3) {
    std::cout << kUsage << std::endl;
    return -1;
  }

  return stratum::hal::dummy::Main(argc, argv);
}