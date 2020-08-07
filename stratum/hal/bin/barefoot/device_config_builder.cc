// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <string>

#include "absl/strings/str_cat.h"
#include "gflags/gflags.h"
#include "nlohmann/json.hpp"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_string(p4c_conf_file, "",
              "Path to the JSON output .conf file of the p4c compiler");
DEFINE_string(bfrt_device_config_text_file, "bfrt_device_config.pb.txt",
              "Path to text file for BfrtDeviceConfig output");
DEFINE_string(bfrt_device_config_binary_file, "bfrt_device_config.pb.bin",
              "Path to file for serialized BfrtDeviceConfig output");
DECLARE_int32(stderrthreshold);
DECLARE_bool(colorlogtostderr);

namespace stratum {
namespace hal {
namespace barefoot {

static ::util::Status Main(int argc, char* argv[]) {
  FLAGS_stderrthreshold = 0;
  FLAGS_colorlogtostderr = true;
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  CHECK_RETURN_IF_FALSE(!FLAGS_p4c_conf_file.empty())
      << "p4c_conf_file must be specified.";

  nlohmann::json conf;
  {
    std::string conf_content;
    RETURN_IF_ERROR(ReadFileToString(FLAGS_p4c_conf_file, &conf_content));
    conf = nlohmann::json::parse(conf_content, nullptr, false);
    CHECK_RETURN_IF_FALSE(!conf.is_discarded()) << "Failed to parse .conf";
    VLOG(1) << ".conf content: " << conf.dump();
  }

  // Translate compiler output JSON conf to protobuf.
  // Taken from bfrt_node.cc
  BfrtDeviceConfig bfrt_config;
  try {
    CHECK_RETURN_IF_FALSE(conf["p4_devices"].size() == 1)
        << "Stratum only supports single devices.";
    // Only support single devices for now.
    const auto& device = conf["p4_devices"][0];
    bfrt_config.set_device(device["device-id"]);
    for (const auto& program : device["p4_programs"]) {
      auto p = bfrt_config.add_programs();
      p->set_name(program["program-name"]);
      LOG(INFO) << "Found P4 program: " << p->name();
      std::string bfrt_content;
      RETURN_IF_ERROR(ReadFileToString(program["bfrt-config"], &bfrt_content));
      p->set_bfrt(bfrt_content);
      ::p4::config::v1::P4Info p4info;
      RETURN_IF_ERROR(ReadProtoFromTextFile(
          absl::StrCat(DirName(program["bfrt-config"]), "/p4info.txt"),
          &p4info));
      *p->mutable_p4info() = p4info;
      for (const auto& pipeline : program["p4_pipelines"]) {
        auto pipe = p->add_pipelines();
        pipe->set_name(pipeline["p4_pipeline_name"]);
        LOG(INFO) << "\tFound pipeline: " << pipe->name();
        for (const auto& scope : pipeline["pipe_scope"]) {
          pipe->add_scope(scope);
        }
        std::string context_content;
        RETURN_IF_ERROR(
            ReadFileToString(pipeline["context"], &context_content));
        pipe->set_context(context_content);
        std::string config_content;
        RETURN_IF_ERROR(ReadFileToString(pipeline["config"], &config_content));
        pipe->set_config(config_content);
      }
    }
  } catch (nlohmann::json::exception& e) {
    return MAKE_ERROR(ERR_INTERNAL) << e.what();
  }

  RETURN_IF_ERROR(
      WriteProtoToTextFile(bfrt_config, FLAGS_bfrt_device_config_text_file));
  RETURN_IF_ERROR(
      WriteProtoToBinFile(bfrt_config, FLAGS_bfrt_device_config_binary_file));

  return ::util::OkStatus();
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::hal::barefoot::Main(argc, argv).error_code();
}
