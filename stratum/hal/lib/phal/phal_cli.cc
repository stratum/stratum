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
  ::util::Status HandleGet(const string& query) {
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

  // Subscribes to the given path into the PHAL attribute database and prints 
  // the stream of results to std::cout. Also prints timing stats for 
  // generating and executing the subscribe. Only returns failure if the 
  // given query path does not match the database schema.
  ::util::Status HandleSubscribe(const string& query) {
    SubscribeRequest req;
    SubscribeResponse resp;
    grpc::ClientContext context;

    // Grab the polling interval
    std::string str;
    int polling_interval = 5;
    while (true) {
        std::cout << "Polling interval(secs) 5: ";
        std::getline(std::cin, str);
        if (std::cin.eof() || str == "") break;
        polling_interval = stoi(str);
        if (polling_interval > 0) break;
        std::cout << "Invalid polling interval: " << str << std::endl;
    }

    // Grab the of responses (0 is infinite)
    int num_responses = 10;
    while (true) {
        std::cout << "Num responses(0 = infinite) 10: ";
        std::getline(std::cin, str);
        if (std::cin.eof() || str == "") break;
        num_responses = stoi(str);
        if (num_responses >= 0) break;
        std::cout << "Invalid num responses: " << str << std::endl;
    }

    req.set_str(query);
    req.set_polling_interval(polling_interval);

    absl::Time start_time = absl::Now();
    std::unique_ptr<grpc::ClientReader<SubscribeResponse>> reader(
        phaldb_svc_->Subscribe(&context, req));

    // Read the stream of responses (Note: return after 10)
    int cnt = num_responses;
    while (reader->Read(&resp)) {

        auto result_str = resp.DebugString();
        if (result_str.size() <= 0) {
            std::cout << "No Results" << std::endl;
        } else {
            std::cout << result_str << std::endl;
        }
        int resp_duration = (absl::Now() - start_time) / 
                                absl::Microseconds(1);
        std::cout << "Response in " << resp_duration << " us."
              << std::endl;
        start_time = absl::Now();

        if (num_responses > 0 && --cnt == 0) break;
    }

    auto status = reader->Finish();
            
    // RPC failed
    if (!status.ok()) {
      return MAKE_ERROR(ERR_INTERNAL)
        << "Subscribe rpc failed: "
        << status.error_message();
    }

    std::cout << "Subscribe finished" << std::endl;

    return ::util::OkStatus();
  }

  // Sets an attribute given the path into the PHAL attribute database.
  ::util::Status HandleSet(const string& query) {
    SetRequest req;
    SetResponse resp;
    grpc::ClientContext context;

    // Create Set request
    auto update = req.add_updates();
    update->set_str(query);

    // Grab value and set it based on type
    while (true) {
        // Grab value
        std::string val_str;
        std::cout << "Value: ";
        std::getline(std::cin, val_str);
        if (val_str == "") {
            std::cout << "Invalid value." << std::endl;
            continue;
        }
        // Grab type and then set it in the request
        std::string type;
        std::cout << "Type: ";
        std::getline(std::cin, type);

        if (!strcmp(type.c_str(), "int32")) {
            ::google::protobuf::int32 val;
            if (sscanf(val_str.c_str(), "%d", &val) != 1) {
                std::cout << "Invalid int32" << std::endl;
                continue;
            }
            update->mutable_value()->set_int32_val(val);
            break;
            
        } else if (!strcmp(type.c_str(), "int64")) {
            ::google::protobuf::int64 val;
            if (sscanf(val_str.c_str(), "%ld", &val) != 1) {
                std::cout << "Invalid int64" << std::endl;
                continue;
            }
            update->mutable_value()->set_int64_val(val);
            break;
            
        } else if (!strcmp(type.c_str(), "uint32")) {
            ::google::protobuf::uint32 val;
            if (sscanf(val_str.c_str(), "%d", &val) != 1) {
                std::cout << "Invalid uint32" << std::endl;
                continue;
            }
            update->mutable_value()->set_uint32_val(val);
            break;
            
        } else if (!strcmp(type.c_str(), "uint64")) {
            ::google::protobuf::uint64 val;
            if (sscanf(val_str.c_str(), "%ld", &val) != 1) {
                std::cout << "Invalid uint64" << std::endl;
                continue;
            }
            update->mutable_value()->set_uint64_val(val);
            break;
            
        } else if (!strcmp(type.c_str(), "double")) {
            double val;
            if (sscanf(val_str.c_str(), "%lf", &val) != 1) {
                std::cout << "Invalid double" << std::endl;
                continue;
            }
            update->mutable_value()->set_double_val(val);
            break;
            
        } else if (!strcmp(type.c_str(), "float")) {
            float val;
            if (sscanf(val_str.c_str(), "%f", &val) != 1) {
                std::cout << "Invalid float" << std::endl;
                continue;
            }
            update->mutable_value()->set_float_val(val);
            break;
            
        } else if (!strcmp(type.c_str(), "bool")) {
            if (!strcmp(val_str.c_str(), "false")) {
                update->mutable_value()->set_bool_val(false);
                break;

            } else if (!strcmp(val_str.c_str(), "true")) {
                update->mutable_value()->set_bool_val(true);
                break;

            } else {
                std::cout << "Invalid bool <false|true>" << std::endl;
                continue;
            }
            
        } else if (!strcmp(type.c_str(), "string")) {
            update->mutable_value()->set_string_val(val_str);
            break;
            
        } else if (!strcmp(type.c_str(), "bytes")) {
            update->mutable_value()->set_bytes_val(val_str);
            
        } else {
            std::cout << "Must specify a type: <int32, uint32, int64, "
                      << "uint64, double, float, bool, string, bytes"
                      << std::endl;
            continue;
        }

    }

    absl::Time start_time = absl::Now();
    auto status = phaldb_svc_->Set(&context, req, &resp);
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

  enum cmd_type {Get, Subscribe, Set};

  // Runs the main CLI loop.
  ::util::Status  RunCli() {

    while (true) {

      cmd_type cmdtype = Get;

      // What type of cmd
      std::string type;
      std::cout << "Cmd type <get, subscribe, set>: ";
      std::getline(std::cin, type);
      if (std::cin.eof()) break;
      if (type == "") {
          std::cout << "Use ^D to quit." << std::endl;
          continue;
      }

      if (!strcmp(type.c_str(), "get")) {
          cmdtype = Get;

      } else if (!strcmp(type.c_str(), "set")) {
          cmdtype = Set;

      } else if (!strcmp(type.c_str(), "subscribe")) {
          cmdtype = Subscribe;

      } else {
          std::cout << "Invalid cmd type " << type << std::endl;
          continue;
      }

      // Grab the path
      std::string query;
      std::cout << "Enter a PHAL path: ";
      std::getline(std::cin, query);
      if (std::cin.eof()) break;
      if (query == "") {
          std::cout << "Use ^D to quit." << std::endl;
          continue;
      }

      ::util::Status result;
      switch (cmdtype) {
      case Get:
          result = HandleGet(query);
          break;

      case Set:
          result = HandleSet(query);
          break;

      case Subscribe:
          result = HandleSubscribe(query);
          break;
      }
      if (!result.ok()) {
          std::cerr << "ERROR: " << type << " Failed: "
                    << result.error_message() << std::endl;
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
