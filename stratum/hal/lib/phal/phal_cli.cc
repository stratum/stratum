// Copyright 2018 Google LLC
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


#include "base/commandlineflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/system_real.h"
#include "stratum/lib/macros.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

DEFINE_string(legacy_phal_config_path, "",
              "The path to read the LegacyPhalInitConfig proto from.");

namespace stratum {
namespace hal {
namespace phal {

::util::Status Main(int argc, char** argv) {
  InitGoogle("phal_cli --legacy_phal_config_path <config_path>", &argc, &argv,
             true);
  if (FLAGS_legacy_phal_config_path.empty())
    return MAKE_ERROR() << "Must provide a legacy_phal_config_path argument.";

  ASSIGN_OR_RETURN(auto database_interface,
                   AttributeDatabase::MakeGoogle(FLAGS_legacy_phal_config_path,
                                                 SystemReal::GetSingleton()));
  // TODO: Provide some sort of REPL.
  absl::SleepFor(absl::Seconds(5));
  LOG(INFO) << "Exiting.";
  return ::util::OkStatus();
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  ::util::Status status = google::hercules::hal::phal::Main(argc, argv);
  if (status.ok())
    return 0;
  else
    return 1;
}
