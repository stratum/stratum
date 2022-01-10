// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#include <iostream>
#include <memory>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/security/tls_credentials_options.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "re2/re2.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/p4/forwarding_pipeline_configs.pb.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_string(grpc_addr, stratum::kLocalStratumUrl,
              "P4Runtime server address.");
DEFINE_string(pipeline_cfg, "pipeline_cfg.pb.txt", "The pipeline config file.");
DEFINE_string(election_id, "0,1",
              "Election id for arbitration update (high,low).");
DEFINE_uint64(device_id, 1, "P4Runtime device ID.");
DEFINE_string(
    ca_cert_file, "",
    "Path to CA certificate, will use insecure credentials if empty.");
DEFINE_string(client_cert_file, "", "Path to client certificate (optional).");
DEFINE_string(client_key_file, "", "Path to client key (optional).");

namespace stratum {
namespace tools {
namespace p4rt_replay {

const char kUsage[] = R"USAGE(
Usage: stratum_replay [options] [p4runtime write log file]
  This tool replays P4Runtime write requests to a Stratum device from a given
  Stratum P4Runtime write request log.
)USAGE";

using ClientStreamChannelReaderWriter =
    ::grpc::ClientReaderWriter<::p4::v1::StreamMessageRequest,
                               ::p4::v1::StreamMessageResponse>;

::util::Status Main(int argc, char** argv) {
  if (argc < 2) {
    LOG(INFO) << kUsage;
    RETURN_ERROR(ERR_INVALID_PARAM).without_logging() << "";
  }

  // Initialize the gRPC channel and P4Runtime service stub
  std::shared_ptr<::grpc::ChannelCredentials> channel_credentials;
  if (!FLAGS_ca_cert_file.empty()) {
    ASSIGN_OR_RETURN(
        channel_credentials,
        CreateSecureClientGrpcChannelCredentials(
            FLAGS_client_key_file, FLAGS_client_cert_file, FLAGS_ca_cert_file));
  } else {
    channel_credentials = ::grpc::InsecureChannelCredentials();
  }
  auto channel = ::grpc::CreateChannel(FLAGS_grpc_addr, channel_credentials);
  auto stub = ::p4::v1::P4Runtime::NewStub(channel);

  // Sends the arbitration update with given device id and election id.
  ::p4::v1::StreamMessageRequest stream_req;
  std::vector<std::string> election_ids =
      absl::StrSplit(FLAGS_election_id, ",");
  CHECK_RETURN_IF_FALSE(election_ids.size() == 2) << "Invalid election ID.";
  uint64 election_id_high;
  uint64 election_id_low;
  CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(election_ids[0], &election_id_high))
      << "Unable to parse string " << election_ids[0] << " to uint64";
  CHECK_RETURN_IF_FALSE(absl::SimpleAtoi(election_ids[1], &election_id_low))
      << "Unable to parse string " << election_ids[1] << " to uint64";
  absl::uint128 election_id =
      absl::MakeUint128(election_id_high, election_id_low);

  stream_req.mutable_arbitration()->set_device_id(FLAGS_device_id);
  stream_req.mutable_arbitration()->mutable_election_id()->set_high(
      absl::Uint128High64(election_id));
  stream_req.mutable_arbitration()->mutable_election_id()->set_low(
      absl::Uint128Low64(election_id));

  ::grpc::ClientContext context;
  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      stub->StreamChannel(&context);
  if (!stream->Write(stream_req)) {
    RETURN_ERROR(ERR_INTERNAL)
        << "Failed to send request '" << stream_req.ShortDebugString()
        << "' to switch.";
  }

  // Push the given pipeline config.
  ::p4::v1::SetForwardingPipelineConfigRequest fwd_pipe_cfg_req;
  ::p4::v1::SetForwardingPipelineConfigResponse fwd_pipe_cfg_resp;
  fwd_pipe_cfg_req.set_device_id(FLAGS_device_id);
  fwd_pipe_cfg_req.mutable_election_id()->set_high(
      absl::Uint128High64(election_id));
  fwd_pipe_cfg_req.mutable_election_id()->set_low(
      absl::Uint128Low64(election_id));
  fwd_pipe_cfg_req.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);

  ::stratum::hal::ForwardingPipelineConfigs pipeline_cfg;
  RETURN_IF_ERROR(ReadProtoFromTextFile(FLAGS_pipeline_cfg, &pipeline_cfg));
  const ::p4::v1::ForwardingPipelineConfig* fwd_pipe_cfg =
      gtl::FindOrNull(pipeline_cfg.node_id_to_config(), FLAGS_device_id);
  CHECK_RETURN_IF_FALSE(fwd_pipe_cfg);
  fwd_pipe_cfg_req.mutable_config()->CopyFrom(*fwd_pipe_cfg);

  ::grpc::Status status;
  {
    ::grpc::ClientContext context;
    status = stub->SetForwardingPipelineConfig(&context, fwd_pipe_cfg_req,
                                               &fwd_pipe_cfg_resp);
    CHECK_RETURN_IF_FALSE(status.ok())
        << "Failed to push forwarding pipeline config: "
        << ::stratum::hal::P4RuntimeGrpcStatusToString(status);
  }

  // Parse the P4Runtime write log file and send write requests to the
  // target device.
  std::string p4_write_logs;
  RETURN_IF_ERROR(::stratum::ReadFileToString(argv[1], &p4_write_logs));
  std::vector<std::string> lines =
      absl::StrSplit(p4_write_logs, '\n', absl::SkipEmpty());
  for (const std::string& line : lines) {
    ::p4::v1::WriteRequest write_req;
    ::p4::v1::WriteResponse write_resp;
    // Log format: <timestamp>;<node_id>;<update proto>;<status>
    // This regular expression contains 4 sub-match groups which extracts
    // elements from the log string. See LogWriteRequest() in P4Service.
    RE2 write_req_regex(
        "(\\d{4}-\\d{1,2}-\\d{1,2} "
        "\\d{1,2}:\\d{1,2}:\\d{1,2}\\.\\d{6});(\\d+);(type[^;]*);(.*)");

    std::string write_request_text;
    std::string error_msg;
    if (!RE2::FullMatch(line, write_req_regex, nullptr, nullptr,
                        &write_request_text, &error_msg)) {
      // Can not find what we want in this line.
      LOG(ERROR) << "Unable to find write request message, skip: " << line;
      continue;
    }

    auto update = write_req.add_updates();
    RETURN_IF_ERROR(ParseProtoFromString(write_request_text, update));
    write_req.set_device_id(FLAGS_device_id);
    write_req.mutable_election_id()->set_high(absl::Uint128High64(election_id));
    write_req.mutable_election_id()->set_low(absl::Uint128Low64(election_id));
    VLOG(1) << "Sending request " << write_req.DebugString();
    ::grpc::ClientContext context;
    status = stub->Write(&context, write_req, &write_resp);

    if (!error_msg.empty()) {
      // Here we expect to get an error, since we only send one update per
      // write request, all we need to do is to check the first error detail.
      // For now, we only show the message if there is an error instead of
      // return with an error status.
      if (status.ok()) {
        LOG(WARNING) << "Expect to get an error, but the request succeeded.\n"
                     << "Expected error: " << error_msg << "\n"
                     << "Request: " << write_req.ShortDebugString();
      } else {
        ::google::rpc::Status details;
        CHECK_RETURN_IF_FALSE(details.ParseFromString(status.error_details()))
            << "Failed to parse error details from gRPC status.";
        if (details.details_size() != 0) {
          ::p4::v1::Error detail;
          CHECK_RETURN_IF_FALSE(details.details(0).UnpackTo(&detail))
              << "Failed to parse the P4Runtime error from detail message.";
          if (detail.message() != error_msg) {
            LOG(WARNING) << "The expected error message is different "
                            "to the actual error message:\n"
                         << "Expected: " << error_msg << "\n"
                         << "Actual: " << detail.message();
          }
        }
      }
    } else {
      CHECK_RETURN_IF_FALSE(status.ok())
          << "Failed to send P4Runtime write request: "
          << write_req.ShortDebugString() << "\n"
          << ::stratum::hal::P4RuntimeGrpcStatusToString(status);
    }
  }

  LOG(INFO) << "Done";
  return ::util::OkStatus();
}

}  // namespace p4rt_replay
}  // namespace tools
}  // namespace stratum

int main(int argc, char** argv) {
  ::gflags::SetUsageMessage(stratum::tools::p4rt_replay::kUsage);
  InitGoogle(argv[0], &argc, &argv, true);
  stratum::InitStratumLogging();
  return stratum::tools::p4rt_replay::Main(argc, argv).error_code();
}
