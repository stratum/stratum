// Copyright 2018 Google LLC
// Copyright 2019 Dell EMC
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


#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "gflags/gflags.h"
#include <grpcpp/grpcpp.h>
#include "absl/strings/str_split.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/macros.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/memory/memory.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/db.grpc.pb.h"

DEFINE_string(stratum_url, "localhost:50051", "URL to the stratum server.");

using namespace std;

namespace stratum {
namespace hal {
namespace phal {

// Handles various CLI interactions with an attribute database.
class PhalCli {
 public:
  // All CLI queries are run on the given attribute database.
  PhalCli(std::shared_ptr<grpc::Channel> channel) 
    : phaldb_svc_(PhalDBSvc::NewStub(channel)) {

  }

  // Queries the given path into the PHAL attribute database and prints the
  // result to std::cout. Also prints timing stats for generating and executing
  // the query. Only returns failure if the given query path does not match the
  // database schema.
  ::util::Status HandleQuery(const string& query) {
    GetRequest req;
    req.set_str(query);
    GetResponse resp;
    grpc::ClientContext context;

    absl::Time start_time = absl::Now();
    auto status = phaldb_svc_->Get(&context, req, &resp);
    absl::Time execute_time = absl::Now();

    // RPC failed
    if (!status.ok()) {
      return MAKE_ERROR(ERR_INTERNAL)
        << "gRPC Get call failed: "
        << status.error_message();
    }

    int execute_duration =
        (execute_time - start_time) / absl::Microseconds(1);

    auto result_str = resp.DebugString();
    if (result_str.size() <= 0) {
      std::cout << "No Results" << std::endl;
    } else {
      std::cout << result_str << std::endl;
    }
    std::cout << "Executed query in " << execute_duration << " us."
              << std::endl;
    return ::util::OkStatus();
  }

  // Runs the main CLI loop.
  ::util::Status  RunCli() {
    while (true) {  // Grap input from std::cin and pass it to a OnlpPhalCli.
      std::string query;
      std::cout << "Enter a PHAL path: ";
      std::getline(std::cin, query);
      if (std::cin.eof()) break;
      if (query == "") {
        std::cout << "Use ^D to quit." << std::endl;
      } else {
        ::util::Status result = HandleQuery(query);
        if (!result.ok()) {
          std::cerr << "ERROR: Query Failed: "
                    << result.error_message() << std::endl;
          continue;
        }
      }
    }

    std::cout << "Exiting." << std::endl;

    return ::util::OkStatus();
  }

 private:
  std::unique_ptr<PhalDBSvc::Stub> phaldb_svc_;
};

::util::Status Main(int argc, char** argv) {
  InitGoogle("phal_cli --stratum_url <url>", &argc, &argv, true);
  stratum::InitStratumLogging();

  PhalCli cli(
    grpc::CreateChannel(FLAGS_stratum_url, 
                        grpc::InsecureChannelCredentials()));

  cli.RunCli();

  return ::util::OkStatus();
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  ::util::Status status = stratum::hal::phal::Main(argc, argv);
  if (status.ok()) {
    return 0;
  } else {
    LOG(ERROR) << status;
    return 1;
  }
}
