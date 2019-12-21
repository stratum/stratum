// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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
#include "absl/strings/str_split.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/onlp/switch_configurator.h"
#include "stratum/hal/lib/phal/onlp/onlpphal.h"
#include "stratum/lib/macros.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "re2/re2.h"

using namespace std;  // NOLINT

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

// Handles various CLI interactions with an attribute database.
class OnlpPhalCli {
 public:
  // All CLI queries are run on the given attribute database.
  explicit OnlpPhalCli(OnlpPhal* onlpphal)
      : onlpphal_(onlpphal) {}

  // Reads the given string into a PHAL query. Returns a failure if the given
  // string uses invalid syntax. This does not guarantee that it is a valid path
  // into the PHAL database.
  //
  // The given string should consiste of at least one '/' separated field. Each
  // field is an attribute group or attribute name followed by an optional
  // index. The index is bracketed, and consists of either a non-negative
  // integer or '@' indicating all indices. The last field may optionally end
  // with a '/' to indicate a terminal group.
  //
  // Valid examples:
  //     "foo/bar[1]/attr"
  //     "foo/bar[@]/attr"
  //     "foo/bar[1]/"  (query everything under bar[1])
  //
  // Invalid examples:
  //     "/"  (at least one field is required)
  //     "foo//bar"
  //     "foo/bar[-1]/"
  ::util::StatusOr<Path> ParseQuery(const std::string& query) {
    std::vector<std::string> query_fields = absl::StrSplit(query, "/");
    bool use_terminal_group = false;
    if (query_fields[query_fields.size() - 1] == "") {
      use_terminal_group = true;  // Query ends with a '/'
      query_fields = {query_fields.begin(),
                      query_fields.begin() + query_fields.size() - 1};
    }

    Path query_path;
    for (const auto& query_field : query_fields) {
      CHECK_RETURN_IF_FALSE(query_field != "")
          << "Encountered unexpected empty query field.";
      RE2 field_regex(R"#((\w+)(\[(?:\d+|\@)\])?)#");
      RE2 bracket_regex(R"#(\[(\d+)\])#");

      PathEntry entry;

      std::string bracket_match;
      CHECK_RETURN_IF_FALSE(
          RE2::FullMatch(query_field, field_regex, &entry.name, &bracket_match))
          << "Could not parse query field: " << query_field;
      if (!bracket_match.empty()) {
        entry.indexed = true;
        if (!RE2::FullMatch(bracket_match, bracket_regex, &entry.index))
          entry.all = true;
      }
      query_path.push_back(entry);
    }

    query_path[query_path.size() - 1].terminal_group = use_terminal_group;
    return query_path;
  }

  // Queries the given path into the PHAL attribute database and prints the
  // result to std::cout. Also prints timing stats for generating and executing
  // the query. Only returns failure if the given query path does not match the
  // database schema.
  ::util::Status HandleQuery(const Path& path) {
    absl::Time start_time = absl::Now();
    ASSIGN_OR_RETURN(auto db_query, onlpphal_->database_->MakeQuery({path}));
    absl::Time generate_time = absl::Now();
    ASSIGN_OR_RETURN(auto result, db_query->Get());
    absl::Time execute_time = absl::Now();

    int generate_duration =
        (generate_time - start_time) / absl::Microseconds(1);
    int execute_duration =
        (execute_time - generate_time) / absl::Microseconds(1);
    auto result_str = result->DebugString();
    if (result_str.size() <= 0) {
      std::cout << "No Results" << std::endl;
    } else {
      std::cout << result_str << std::endl;
    }
    std::cout << "Generated query in " << generate_duration << " us."
              << std::endl;
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
        ::util::StatusOr<Path> path = ParseQuery(query);
        if (!path.ok()) {
          std::cerr << "ERROR: Failed to generate query: " << path.status();
          continue;
        }
        ::util::Status result = HandleQuery(path.ValueOrDie());
        if (!result.ok()) {
          std::cerr << "ERROR: Failed to execute query (this is a bug!): "
                    << result;
          continue;
        }
      }
    }

    std::cout << "Exiting." << std::endl;

    return ::util::OkStatus();
  }

 private:
  const OnlpPhal* onlpphal_;
};

::util::Status Main(int argc, char** argv) {
  InitGoogle("onlpphal_cli --phal_config_path <config_path>", &argc, &argv,
             true);
  stratum::InitStratumLogging();

  // Need to init Onlp Interface
  auto onlp_wrapper = OnlpWrapper::Make().ConsumeValueOrDie();
  auto onlpphal = OnlpPhal::CreateSingleton(onlp_wrapper.get());

  OnlpPhalCli cli(onlpphal);
  cli.RunCli();

  // Shutdown the ONLP Phal
  onlpphal->Shutdown();

  return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  ::util::Status status = stratum::hal::phal::onlp::Main(argc, argv);
  if (status.ok()) {
    return 0;
  } else {
    LOG(ERROR) << status;
    return 1;
  }
}
