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
#include "stratum/hal/lib/barefoot/bf.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_string(p4c_conf_file, "",
              "Path to the JSON output .conf file of the bf-p4c compiler");
DEFINE_string(bf_pipeline_config_text_file, "bf_pipeline_config.pb.txt",
              "Path to text file for BfPipelineConfig output");
DEFINE_string(bf_pipeline_config_binary_file, "bf_pipeline_config.pb.bin",
              "Path to file for serialized BfPipelineConfig output");
DEFINE_string(unpack_dir, "",
              "Directory to recreate the compiler output from the serialized "
              "BfPipelineConfig by unpacking the files to disk");

namespace stratum {
namespace hal {
namespace barefoot {

namespace {
constexpr char kUsage[] =
    R"USAGE(usage: -p4c_conf_file=/path/to/bf-p4c/output/program.conf -bf_pipeline_config_binary_file=$PWD/bf-pipeline.pb.bin

This program assembles a Stratum-bf pipeline protobuf message from the output of
the Barefoot P4 compiler. This message can be pushed to Stratum in the
p4_device_config field of the P4Runtime SetForwardingPipelineConfig message.
)USAGE";

::util::Status Unpack() {
  CHECK_RETURN_IF_FALSE(!FLAGS_bf_pipeline_config_binary_file.empty())
      << "pipeline_config_binary_file must be specified.";

  BfPipelineConfig bf_config;
  RETURN_IF_ERROR(
      ReadProtoFromBinFile(FLAGS_bf_pipeline_config_binary_file, &bf_config));

  // TODO(max): replace with <filesystem> once we move to C++17
  char* resolved_path = realpath(FLAGS_unpack_dir.c_str(), nullptr);
  if (!resolved_path) {
    RETURN_ERROR(ERR_INTERNAL) << "Unable to resolve path " << FLAGS_unpack_dir;
  }
  std::string base_path(resolved_path);
  free(resolved_path);

  for (const auto& program : bf_config.programs()) {
    CHECK_RETURN_IF_FALSE(!program.name().empty());
    LOG(INFO) << "Found P4 program: " << program.name();
    RETURN_IF_ERROR(
        RecursivelyCreateDir(absl::StrCat(base_path, "/", program.name())));
    RETURN_IF_ERROR(WriteStringToFile(
        program.bfrt(),
        absl::StrCat(base_path, "/", program.name(), "/", "bfrt.json")));
    RETURN_IF_ERROR(WriteProtoToTextFile(
        program.p4info(),
        absl::StrCat(base_path, "/", program.name(), "/", "p4info.txt")));
    for (const auto& pipeline : program.pipelines()) {
      CHECK_RETURN_IF_FALSE(!pipeline.name().empty());
      LOG(INFO) << "\tFound pipeline: " << pipeline.name();
      RETURN_IF_ERROR(RecursivelyCreateDir(
          absl::StrCat(base_path, "/", program.name(), "/", pipeline.name())));
      RETURN_IF_ERROR(WriteStringToFile(
          pipeline.context(),
          absl::StrCat(base_path, "/", program.name(), "/", pipeline.name(),
                       "/", "context.json")));
      RETURN_IF_ERROR(WriteStringToFile(
          pipeline.config(), absl::StrCat(base_path, "/", program.name(), "/",
                                          pipeline.name(), "/", "tofino.bin")));
    }
  }

  return ::util::OkStatus();
}
}  // namespace

static ::util::Status Main(int argc, char* argv[]) {
  ::gflags::SetUsageMessage(kUsage);
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  if (!FLAGS_unpack_dir.empty()) {
    return Unpack();
  }

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
  // Taken from bf_pipeline_utils.cc
  BfPipelineConfig bf_config;
  try {
    CHECK_RETURN_IF_FALSE(conf["p4_devices"].size() == 1)
        << "Stratum only supports single devices.";
    // Only support single devices for now.
    const auto& device = conf["p4_devices"][0];
    for (const auto& program : device["p4_programs"]) {
      auto p = bf_config.add_programs();
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
      WriteProtoToTextFile(bf_config, FLAGS_bf_pipeline_config_text_file));
  RETURN_IF_ERROR(
      WriteProtoToBinFile(bf_config, FLAGS_bf_pipeline_config_binary_file));

  return ::util::OkStatus();
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::hal::barefoot::Main(argc, argv).error_code();
}
