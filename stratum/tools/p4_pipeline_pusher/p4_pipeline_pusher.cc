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
DEFINE_uint64(election_id, 1,
              "Election ID for the controller instance. Will be used in all "
              "P4Runtime RPCs sent to the switch. Note that election_id is 128 "
              "bits, but here we assume we only give the lower 64 bits only.");
DEFINE_string(ca_cert_file, "",
              "CA certificate, will use insecure credentials if empty.");
DEFINE_string(client_cert_file, "", "Client certificate (optional).");
DEFINE_string(client_key_file, "", "Client key (optional).");

namespace stratum {
namespace tools {
namespace p4_pipeline_pusher {

namespace {

const char kUsage[] =
    R"USAGE(push a pipeline to a P4 device over P4Runtime)USAGE";

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
  std::shared_ptr<::grpc::ChannelCredentials> channel_credentials;
  if (!FLAGS_ca_cert_file.empty()) {
    ASSIGN_OR_RETURN(
        channel_credentials,
        CreateSecureClientGrpcChannelCredentials(
            FLAGS_client_key_file, FLAGS_client_cert_file, FLAGS_ca_cert_file));
  } else {
    channel_credentials = ::grpc::InsecureChannelCredentials();
  }
  ASSIGN_OR_RETURN(auto p4rt_session, p4runtime::P4RuntimeSession::Create(
                                          FLAGS_grpc_addr, channel_credentials,
                                          FLAGS_device_id, FLAGS_election_id));
  RETURN_IF_ERROR(
      p4rt_session->SetForwardingPipelineConfig(p4info, p4_device_config));

  return ::util::OkStatus();
}

}  // namespace
}  // namespace p4_pipeline_pusher
}  // namespace tools
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::tools::p4_pipeline_pusher::Main(argc, argv).error_code();
}
