// Copyright 2019 Open Networking Foundation
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

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gflags/gflags.h"
#include "grpcpp/grpcpp.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/db.grpc.pb.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"

DEFINE_string(phal_db_url, stratum::kPhalDbServiceUrl,
              "URL to the phalDb server.");

namespace stratum {
namespace hal {
namespace phal {

namespace {
// Parse PB Query string to Phal DB Path
::util::StatusOr<PathQuery> ParseQuery(const std::string& query) {
  PathQuery path_query;
  std::vector<std::string> query_fields =
      absl::StrSplit(query, "/" /*, absl::SkipEmpty()*/);

  CHECK(query_fields.size() != 0) << "Invalid query string";

  bool use_terminal_group = false;
  // Double-check this check because of SkipEmpty
  if (query_fields.back() == "") {
    use_terminal_group = true;  // Query ends with a '/'
    query_fields.pop_back();
  }

  for (const auto& field : query_fields) {
    CHECK_RETURN_IF_FALSE(field != "")
        << "Encountered unexpected empty query field.";
    PathQuery::PathEntry entry;
    entry.set_name(field);
    *path_query.add_entries() = entry;
  }

  path_query.mutable_entries()->rbegin()->set_terminal_group(
      use_terminal_group);

  return path_query;
}
}  // namespace

// Handles various CLI interactions with an attribute database.
class PhalCli {
 public:
  // All CLI queries are run on the given attribute database.
  explicit PhalCli(std::shared_ptr<grpc::Channel> channel)
      : phaldb_svc_(PhalDb::NewStub(channel)) {}

  // Queries the given path into the PHAL attribute database and prints the
  // result to std::cout. Also prints timing stats for generating and executing
  // the query. Only returns failure if the given query path does not match the
  // database schema.
  ::util::Status HandleGet(const std::string& query) {
    GetRequest req;
    ASSIGN_OR_RETURN(auto path, ParseQuery(query));
    *req.mutable_path() = path;
    GetResponse resp;
    grpc::ClientContext context;

    absl::Time start_time = absl::Now();
    auto status = phaldb_svc_->Get(&context, req, &resp);
    absl::Time execute_time = absl::Now();

    // RPC failed
    if (!status.ok()) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "gRPC Get call failed: " << status.error_message();
    }

    int64_t execute_duration =
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
  ::util::Status HandleSubscribe(const std::string& query) {
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

    ASSIGN_OR_RETURN(auto path, ParseQuery(query));
    *req.mutable_path() = path;
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
      int resp_duration = (absl::Now() - start_time) / absl::Microseconds(1);
      std::cout << "Response in " << resp_duration << " us." << std::endl;
      start_time = absl::Now();

      if (num_responses > 0 && --cnt == 0) {
        std::cout << "Reached max polls" << std::endl;
        return ::util::OkStatus();
      }
    }

    auto status = reader->Finish();

    // RPC failed
    if (!status.ok()) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Subscribe rpc failed: " << status.error_message();
    }

    std::cout << "Subscribe finished" << std::endl;

    return ::util::OkStatus();
  }

  // Sets an attribute given the path into the PHAL attribute database.
  ::util::Status HandleSet(const std::string& query) {
    SetRequest req;
    SetResponse resp;
    grpc::ClientContext context;

    // Create Set request
    auto update = req.add_updates();
    ASSIGN_OR_RETURN(auto path, ParseQuery(query));
    *update->mutable_path() = path;

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

      if (type.compare("int32") == 0) {
        int32_t val;
        if (sscanf(val_str.c_str(), "%d", &val) != 1) {
          std::cout << "Invalid int32" << std::endl;
          continue;
        }
        update->mutable_value()->set_int32_val(val);
        break;

      } else if (type.compare("int64") == 0) {
        int64_t val;
        if (sscanf(val_str.c_str(), "%ld", &val) != 1) {
          std::cout << "Invalid int64" << std::endl;
          continue;
        }
        update->mutable_value()->set_int64_val(val);
        break;

      } else if (type.compare("uint32") == 0) {
        uint32_t val;
        if (sscanf(val_str.c_str(), "%d", &val) != 1) {
          std::cout << "Invalid uint32" << std::endl;
          continue;
        }
        update->mutable_value()->set_uint32_val(val);
        break;

      } else if (type.compare("uint64") == 0) {
        uint64_t val;
        if (sscanf(val_str.c_str(), "%ld", &val) != 1) {
          std::cout << "Invalid uint64" << std::endl;
          continue;
        }
        update->mutable_value()->set_uint64_val(val);
        break;

      } else if (type.compare("double") == 0) {
        double val;
        if (sscanf(val_str.c_str(), "%lf", &val) != 1) {
          std::cout << "Invalid double" << std::endl;
          continue;
        }
        update->mutable_value()->set_double_val(val);
        break;

      } else if (type.compare("float") == 0) {
        float val;
        if (sscanf(val_str.c_str(), "%f", &val) != 1) {
          std::cout << "Invalid float" << std::endl;
          continue;
        }
        update->mutable_value()->set_float_val(val);
        break;

      } else if (type.compare("bool") == 0) {
        if (val_str.compare("false") == 0) {
          update->mutable_value()->set_bool_val(false);
          break;

        } else if (val_str.compare("true") == 0) {
          update->mutable_value()->set_bool_val(true);
          break;

        } else {
          std::cout << "Invalid bool <false|true>" << std::endl;
          continue;
        }

      } else if (type.compare("string") == 0) {
        update->mutable_value()->set_string_val(val_str);
        break;

      } else if (type.compare("bytes") == 0) {
        update->mutable_value()->set_bytes_val(val_str);
        break;

      } else {
        std::cout << "Must specify a type: <int32, uint32, int64, "
                  << "uint64, double, float, bool, string, bytes" << std::endl;
        continue;
      }
    }

    absl::Time start_time = absl::Now();
    auto status = phaldb_svc_->Set(&context, req, &resp);
    absl::Time execute_time = absl::Now();

    // RPC failed
    if (!status.ok()) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "gRPC Get call failed: " << status.error_message();
    }

    int64_t execute_duration =
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

  enum cmd_type { CMD_GET, CMD_SUBSCRIBE, CMD_SET };

  // Runs the main CLI loop.
  ::util::Status RunCli() {
    while (true) {
      cmd_type cmdtype = CMD_GET;

      // What type of cmd
      std::string resp;
      std::cout << "Cmd type <get, subscribe, set>: ";
      std::getline(std::cin, resp);
      if (std::cin.eof()) break;

      // Split the string by space
      std::vector<std::string> r = absl::StrSplit(resp, ' ');
      if (r.size() == 0 || r[0] == "") {
        std::cout << "Use ^D to quit." << std::endl;
        continue;
      }
      std::string type = r[0];

      if (type.compare("get") == 0) {
        cmdtype = CMD_GET;

      } else if (type.compare("set") == 0) {
        cmdtype = CMD_SET;

      } else if (type.compare("sub") == 0) {
        cmdtype = CMD_SUBSCRIBE;

      } else {
        std::cout << "Invalid cmd type " << type << std::endl;
        continue;
      }

      // If the query was passed in via the original response use that
      std::string query;
      if (r.size() > 1) {
        query = r[1];

        // Else Grab the query path
      } else {
        std::cout << "Enter a PHAL path: ";
        std::getline(std::cin, query);
        if (std::cin.eof()) break;
      }
      if (query == "") {
        std::cout << "Path nodes: "
                  << "cards|fan_trays|led_groups|psu_trays|thermal_groups"
                  << std::endl;
        std::cout << "Use ^D to quit." << std::endl;
        continue;
      }

      ::util::Status result;
      switch (cmdtype) {
        case CMD_GET:
          result = HandleGet(query);
          break;

        case CMD_SET:
          result = HandleSet(query);
          break;

        case CMD_SUBSCRIBE:
          result = HandleSubscribe(query);
          break;
      }
      if (!result.ok()) {
        std::cerr << "ERROR: " << type << " Failed: " << result.error_message()
                  << std::endl;
      }
    }

    std::cout << "Exiting." << std::endl;

    return ::util::OkStatus();
  }

 private:
  std::unique_ptr<PhalDb::Stub> phaldb_svc_;
};

::util::Status Main(int argc, char** argv) {
  InitGoogle("phal_cli", &argc, &argv, true);
  stratum::InitStratumLogging();

  PhalCli cli(grpc::CreateChannel(FLAGS_phal_db_url,
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
