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

Chassis configuration migration tool.

Combine with xargs for mass migration:
ls -1 stratum/hal/config/*/chassis_config.pb.txt | \
  xargs -n 1 bazel run //stratum/hal/config:tofino_chassis_config_migrator -- \
    -chassis_config_file
)USAGE";

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
        << "Chassis config is not for Tofino platform";
  }
  for (auto& singleton_port : *config.mutable_singleton_ports()) {
    std::vector<std::string> parts = absl::StrSplit(singleton_port.name(), '/');
    CHECK_RETURN_IF_FALSE(parts.size() == 2)
        << "Can't parse port name " << singleton_port.name() << ".";
    int port_id;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(parts[0], &port_id));
    int name_channel;
    CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(parts[1], &name_channel));
    int new_port_id;
    std::string new_port_name;
    if (singleton_port.channel() == 0) {
      // Non-channelized port.
      new_port_id = port_id;
      new_port_name = absl::StrFormat("%i/0", new_port_id);
    } else {
      // Channelized port.
      CHECK_RETURN_IF_FALSE(name_channel + 1 == singleton_port.channel())
          << "channel field does not match port name in singleton port "
          << singleton_port.ShortDebugString() << ".";
      new_port_id = port_id * 100 + name_channel;
      new_port_name = absl::StrFormat("%i/%i", port_id, name_channel);
    }

    singleton_port.set_id(new_port_id);
    singleton_port.set_name(new_port_name);
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
