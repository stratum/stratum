// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/p4runtime/p4runtime_session.h"
#include "stratum/lib/utils.h"

DEFINE_string(grpc_addr, "127.0.0.1:9339", "P4Runtime server address.");
DEFINE_string(p4_info_file, "",
              "Path to an optional P4Info text proto file. If specified, file "
              "content will be serialized into the p4info field in "
              "ForwardingPipelineConfig proto and pushed to the switch.");
DEFINE_string(p4_pipeline_config_file, "",
              "Path to an optional P4PipelineConfig bin proto file. If "
              "specified, file content will be serialized into the "
              "p4_device_config field in ForwardingPipelineConfig proto "
              "and pushed to the switch.");
DEFINE_uint64(device_id, 1, "P4Runtime device ID.");

namespace stratum {
namespace tools {
namespace p4runtime {

namespace {

const char kUsage[] = R"USAGE(todo)USAGE";

::util::Status Main(int argc, char** argv) {
  ::gflags::SetUsageMessage(kUsage);
  InitGoogle(argv[0], &argc, &argv, true);
  stratum::InitStratumLogging();

  CHECK_RETURN_IF_FALSE(!FLAGS_p4_info_file.empty());
  CHECK_RETURN_IF_FALSE(!FLAGS_p4_pipeline_config_file.empty());
  ::p4::config::v1::P4Info p4info;
  RETURN_IF_ERROR(ReadProtoFromTextFile(FLAGS_p4_info_file, &p4info));
  std::string p4_device_config;
  RETURN_IF_ERROR(
      ReadFileToString(FLAGS_p4_pipeline_config_file, &p4_device_config));
  ASSIGN_OR_RETURN(auto p4rt_session,
                   ::stratum::p4runtime::P4RuntimeSession::Create(
                       FLAGS_grpc_addr, ::grpc::InsecureChannelCredentials(),
                       FLAGS_device_id));
  RETURN_IF_ERROR(SetForwardingPipelineConfig(p4rt_session.get(), p4info,
                                              p4_device_config));

  return ::util::OkStatus();
}

}  // namespace
}  // namespace p4runtime
}  // namespace tools
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::tools::p4runtime::Main(argc, argv).error_code();
}
