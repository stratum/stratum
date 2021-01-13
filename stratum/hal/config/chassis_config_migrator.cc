// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <string>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_string(chassis_config_file, "",
              "Path to chassis configuration to migrate.");

namespace stratum {
namespace hal {
namespace config {

const char kUsage[] =
    R"USAGE(usage: --chassis_config_file=<path>

Chassis configuration migration and validation tool.

Combine with xargs for bulk migration:
ls -1 stratum/hal/config/*/chassis_config.pb.txt | \
  xargs -n 1 bazel run //stratum/hal/config:chassis_config_migrator -- \
    -chassis_config_file
)USAGE";

::util::Status MigrateSingletonPort(SingletonPort* singleton_port) {
  // Only change the port name if it matches the <port>/<slot> pattern.
  bool fix_name = true;
  std::vector<std::string> parts = absl::StrSplit(singleton_port->name(), '/');
  if (parts.size() != 2) {
    LOG(WARNING) << "Can't parse port name " << singleton_port->name()
                 << " as <port>/<channel>.";
    fix_name = false;
  }

  // Fix the port id and name based on slot/port/channel.
  if (singleton_port->channel() == 0) {
    // Non-channelized port.
    singleton_port->set_id(singleton_port->port());
    if (fix_name) {
      singleton_port->set_name(absl::StrFormat("%i/0", singleton_port->port()));
    }
  } else {
    // Channelized port.
    singleton_port->set_id(singleton_port->port() * 100 +
                           singleton_port->channel() - 1);
    if (fix_name) {
      singleton_port->set_name(absl::StrFormat("%i/%i", singleton_port->port(),
                                               singleton_port->channel() - 1));
    }
  }

  return ::util::OkStatus();
}

::util::Status Main(int argc, char** argv) {
  ::gflags::SetUsageMessage(kUsage);
  InitGoogle(argv[0], &argc, &argv, true);
  stratum::InitStratumLogging();

  CHECK_RETURN_IF_FALSE(!FLAGS_chassis_config_file.empty())
      << "No chassis config given.";
  ChassisConfig config;
  RETURN_IF_ERROR(ReadProtoFromTextFile(FLAGS_chassis_config_file, &config));
  if (config.chassis().platform() != PLT_GENERIC_BAREFOOT_TOFINO &&
      config.chassis().platform() != PLT_GENERIC_BAREFOOT_TOFINO2) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Chassis config is not for a Tofino platform";
  }
  for (auto& singleton_port : *config.mutable_singleton_ports()) {
    RETURN_IF_ERROR(MigrateSingletonPort(&singleton_port));
  }

  RETURN_IF_ERROR(WriteProtoToTextFile(config, FLAGS_chassis_config_file));

  return ::util::OkStatus();
}

}  // namespace config
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  ::util::Status status = stratum::hal::config::Main(argc, argv);
  if (status.ok()) {
    return 0;
  } else {
    LOG(ERROR) << status;
    return status.error_code();
  }
}
