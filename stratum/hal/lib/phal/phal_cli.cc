// Copyright 2019-present Open Networking Foundation
// Copyright 2019 Dell EMC
// SPDX-License-Identifier: Apache-2.0

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
#include "re2/re2.h"
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
DEFINE_uint64(interval, 5000, "Subscribe poll interval in ms.");
DEFINE_uint64(count, -1, "Subscribe poll count. Default is infinite.");
DEFINE_double(double_val, 0, "Set a double value.");
DEFINE_double(float_val, 0, "Set a float value.");
DEFINE_int32(int32_val, 0, "Set a int32 value.");
DEFINE_int64(int64_val, 0, "Set a int64 value.");
DEFINE_uint64(uint32_val, 0, "Set a uint32 value.");
DEFINE_uint64(uint64_val, 0, "Set a uint64 value.");
DEFINE_bool(bool_val, false, "Set a boolean value.");
DEFINE_string(string_val, "", "Set a string value.");
DEFINE_string(bytes_val, "", "Set a bytes value.");

namespace stratum {
namespace hal {
namespace phal {

namespace {
const char kUsage[] =
    R"USAGE({get,set,sub} path [--<type>_val=<value>]
Basic PHAL CLI. Query the internal state of the Phal database.

Examples:
  Get:
  get cards[0]/ports[0]/transceiver/hardware_state # First port from first card
  get cards[@]/ports[@]/transceiver/hardware_state # All ports from all cards

  Set:
  set fan_trays[0]/fans[0]/speed_control --int32_val 30

  Subscribe:
  sub fan_trays[@]/fans[@]/speed_control --interval=500 --count=2
)USAGE";

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

  for (const auto& query_field : query_fields) {
    CHECK_RETURN_IF_FALSE(query_field != "")
        << "Encountered unexpected empty query field.";

    PathQuery::PathEntry entry;  // Protobuf type
    RE2 field_regex(R"#((\w+)(\[(?:\d+|\@)\])?)#");
    RE2 bracket_regex(R"#(\[(\d+)\])#");
    std::string bracket_match;
    std::string name_match;
    CHECK_RETURN_IF_FALSE(
        RE2::FullMatch(query_field, field_regex, &name_match, &bracket_match))
        << "Could not parse query field: " << query_field;
    entry.set_name(name_match);
    if (!bracket_match.empty()) {
      entry.set_indexed(true);
      int index;
      if (!RE2::FullMatch(bracket_match, bracket_regex, &index))
        entry.set_all(true);
      entry.set_index(index);
    }
    *path_query.add_entries() = entry;
  }

  path_query.mutable_entries()->rbegin()->set_terminal_group(
      use_terminal_group);

  return path_query;
}
}  // namespace

// Handles various CLI interactions with an attribute database.
class PhalCli {
 private:
  enum cmd_type { CMD_GET, CMD_SUBSCRIBE, CMD_SET };

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
        (execute_time - start_time) / absl::Milliseconds(1);

    auto result_str = resp.DebugString();
    if (result_str.size() <= 0) {
      std::cout << "No Results" << std::endl;
    } else {
      std::cout << result_str << std::endl;
    }
    LOG(INFO) << "Executed query in " << execute_duration << " ms.";

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

    ASSIGN_OR_RETURN(auto path, ParseQuery(query));
    *req.mutable_path() = path;
    req.set_polling_interval(
        absl::ToInt64Nanoseconds(absl::Milliseconds(FLAGS_interval)));

    absl::Time start_time = absl::Now();
    std::unique_ptr<grpc::ClientReader<SubscribeResponse>> reader(
        phaldb_svc_->Subscribe(&context, req));

    // Read the stream of responses
    for (int i = 0; i < FLAGS_count && reader->Read(&resp); ++i) {
      std::cout << resp.DebugString() << std::endl;
      int resp_duration = (absl::Now() - start_time) / absl::Milliseconds(1);
      LOG(INFO) << "Response in " << resp_duration << " ms.";
      start_time = absl::Now();
    }
    context.TryCancel();
    auto status = reader->Finish();

    // RPC failed
    if (!status.ok() && status.error_code() != grpc::StatusCode::CANCELLED) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Subscribe rpc failed: " << status.error_message();
    }

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

    if (!::gflags::GetCommandLineFlagInfoOrDie("double_val").is_default) {
      update->mutable_value()->set_double_val(FLAGS_double_val);
    }
    if (!::gflags::GetCommandLineFlagInfoOrDie("float_val").is_default) {
      update->mutable_value()->set_float_val(FLAGS_float_val);
    }
    if (!::gflags::GetCommandLineFlagInfoOrDie("int32_val").is_default) {
      update->mutable_value()->set_int32_val(FLAGS_int32_val);
    }
    if (!::gflags::GetCommandLineFlagInfoOrDie("int64_val").is_default) {
      update->mutable_value()->set_int64_val(FLAGS_int64_val);
    }
    if (!::gflags::GetCommandLineFlagInfoOrDie("uint32_val").is_default) {
      update->mutable_value()->set_uint32_val(FLAGS_uint32_val);
    }
    if (!::gflags::GetCommandLineFlagInfoOrDie("uint64_val").is_default) {
      update->mutable_value()->set_uint64_val(FLAGS_uint64_val);
    }
    if (!::gflags::GetCommandLineFlagInfoOrDie("bool_val").is_default) {
      update->mutable_value()->set_bool_val(FLAGS_bool_val);
    }
    if (!::gflags::GetCommandLineFlagInfoOrDie("string_val").is_default) {
      update->mutable_value()->set_string_val(FLAGS_string_val);
    }
    if (!::gflags::GetCommandLineFlagInfoOrDie("bytes_val").is_default) {
      update->mutable_value()->set_bytes_val(FLAGS_bytes_val);
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
        (execute_time - start_time) / absl::Milliseconds(1);
    std::cout << resp.DebugString() << std::endl;
    LOG(INFO) << "Executed query in " << execute_duration << " us.";

    return ::util::OkStatus();
  }

  ::util::StatusOr<cmd_type> ParseCommand(std::string command) {
    if (command == "get") {
      return CMD_GET;
    } else if (command == "set") {
      return CMD_SET;
    } else if (command == "subscribe" || command == "sub") {
      return CMD_SUBSCRIBE;
    }

    return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid command: " << command;
  }

  // Runs the main CLI loop.
  ::util::Status RunCli(int argc, char** argv) {
    if (argc < 3) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Command and path are required.";
    }

    ASSIGN_OR_RETURN(const auto cmd, ParseCommand(argv[1]));

    switch (cmd) {
      case CMD_GET:
        RETURN_IF_ERROR(HandleGet(argv[2]));
        break;
      case CMD_SET:
        RETURN_IF_ERROR(HandleSet(argv[2]));
        break;
      case CMD_SUBSCRIBE:
        RETURN_IF_ERROR(HandleSubscribe(argv[2]));
        break;
    }

    return ::util::OkStatus();
  }

 private:
  std::unique_ptr<PhalDb::Stub> phaldb_svc_;
};

::util::Status Main(int argc, char** argv) {
  ::gflags::SetUsageMessage(kUsage);
  InitGoogle(argv[0], &argc, &argv, true);
  stratum::InitStratumLogging();
  PhalCli cli(grpc::CreateChannel(FLAGS_phal_db_url,
                                  grpc::InsecureChannelCredentials()));
  cli.RunCli(argc, argv);

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
    return status.error_code();
  }
}
