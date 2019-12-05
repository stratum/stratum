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

#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "stratum/lib/macros.h"

using namespace std;  // NOLINT

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

// Handles various CLI interactions with an attribute database.
class OnlpCli {
 public:
  // All CLI queries are run on the given attribute database.
  OnlpCli() : onlp_interface_(nullptr) {}

  // Print OID List
  ::util::Status PrintOidList(string use_wrapper, std::string stype,
                              onlp_oid_type_flag_t type) {
    // call onlp directly
    if (use_wrapper == "n") {
      biglist_t* oid_hdr_list;

      onlp_oid_hdr_get_all(ONLP_OID_CHASSIS, type, 0, &oid_hdr_list);

      biglist_t* curr_node = oid_hdr_list;
      // std::cout << stype << " OID List:" << std::endl;
      while (curr_node != nullptr) {
        onlp_oid_hdr_t* oid_hdr =
            reinterpret_cast<onlp_oid_hdr_t*>(curr_node->data);
        std::cout << "  oid: " << oid_hdr->id << std::endl;
        curr_node = curr_node->next;
      }
      onlp_oid_get_all_free(oid_hdr_list);

      // using onlp_wrapper
    } else {
      ASSIGN_OR_RETURN(std::vector<OnlpOid> OnlpOids,
                       onlp_interface_->GetOidList(type));
      if (OnlpOids.size() == 0) {
        std::cout << "no " << stype << " OIDs" << std::endl;
        return ::util::OkStatus();
      }
      std::cout << stype << " OID List:" << std::endl;
      for (unsigned int i = 0; i < OnlpOids.size(); i++) {
        std::cout << "  " << i << ": oid: " << OnlpOids[i] << std::endl;
      }
    }
    return ::util::OkStatus();
  }

  // Runs the main CLI loop.
  ::util::Status RunCli() {
    // Create the OnlpInterface object
    ASSIGN_OR_RETURN(onlp_interface_, OnlpWrapper::Make());

    while (true) {  // Grap input from std::cin and pass it to a OnlpCli.
      std::string use_wrapper;
      std::cout << "Use Wrapper (Y/n): ";
      std::string query;
      std::getline(std::cin, query);
      std::cout << "Enter an OID type: ";
      std::getline(std::cin, query);
      if (std::cin.eof()) break;
      if (query == "") {
        std::cout << "Use ^D to quit." << std::endl;
      } else if (query == "chassis") {
        PrintOidList(use_wrapper, query, ONLP_OID_TYPE_FLAG_CHASSIS);
      } else if (query == "module") {
        PrintOidList(use_wrapper, query, ONLP_OID_TYPE_FLAG_MODULE);
      } else if (query == "thermal") {
        PrintOidList(use_wrapper, query, ONLP_OID_TYPE_FLAG_THERMAL);
      } else if (query == "fan") {
        PrintOidList(use_wrapper, query, ONLP_OID_TYPE_FLAG_FAN);
      } else if (query == "psu") {
        PrintOidList(use_wrapper, query, ONLP_OID_TYPE_FLAG_PSU);
      } else if (query == "led") {
        PrintOidList(use_wrapper, query, ONLP_OID_TYPE_FLAG_LED);
      } else if (query == "sfp") {
        PrintOidList(use_wrapper, query, ONLP_OID_TYPE_FLAG_SFP);
      } else if (query == "generic") {
        PrintOidList(use_wrapper, query, ONLP_OID_TYPE_FLAG_GENERIC);
      } else {
        std::cout << "unknown oid type" << std::endl;
      }
    }

    std::cout << "Exiting." << std::endl;

    return ::util::OkStatus();
  }

 private:
  std::unique_ptr<OnlpInterface> onlp_interface_;
};

::util::Status Main(int argc, char** argv) {
  InitGoogle("onlp_cli", &argc, &argv, true);

  OnlpCli cli;
  cli.RunCli();

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
