// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#include <iostream>
#include <memory>
#include <regex>  // NOLINT: [build/c++11]

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/security/tls_credentials_options.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_string(grpc_addr, "127.0.0.1:9339", "P4Runtime server address");
DEFINE_string(p4info, "p4info.pb.txt", "The P4Info file");
DEFINE_string(pipeline_cfg, "pipeline.pb.bin", "The pipeline config file");
DEFINE_string(election_id, "0,1",
              "election id for abstraction update (high,low)");
DEFINE_uint64(device_id, 1, "The device ID.");
DEFINE_string(ca_cert, "",
              "CA certificate, will use insecure credential if empty");
DEFINE_string(client_cert, "", "Client certificate (optional)");
DEFINE_string(client_key, "", "Client key (optional)");
DEFINE_uint32(write_batch_size, 1, "Batch size of write request");

namespace stratum {
namespace tools {
namespace p4rt_replay {

const char kUsage[] = R"USAGE(
Usage: stratum-replay [options] [p4runtime write log file]
  This tool replay P4Runtime write requests to a Stratum device with a given
  Stratum P4Runtime write request log.

  Options:
    -device_id: The device ID (default: 1)
    -election_id: Election ID (high,low) for abstraction update (default: "0,1")
    -grpc_addr: Stratum gRPC address (default: "127.0.0.1:9339")
    -p4info: The P4Info file (default: "p4info.pb.txt")
    -pipeline_cfg: The pipeline config file (default: "pipeline.pb.bin")
    -ca_cert: CA certificate(optional), will use insecure credential if empty (default: "")
    -client_cert: Client certificate (optional) (default: "")
    -client_key: Client key (optional) (default: "")
    -write_batch_size: Max size of P4Runtime updates in a write request (default: 1)
)USAGE";

using ClientStreamChannelReaderWriter =
    ::grpc::ClientReaderWriter<::p4::v1::StreamMessageRequest,
                               ::p4::v1::StreamMessageResponse>;

::util::Status Main(int argc, char** argv) {
  if (argc < 2) {
    LOG(INFO) << kUsage;
    RETURN_ERROR(ERR_INVALID_PARAM).without_logging() << "";
  }

  auto channel_credentials = ::grpc::InsecureChannelCredentials();
  if (!FLAGS_ca_cert.empty()) {
    ::grpc::string pem_root_certs;
    ::grpc_impl::experimental::TlsKeyMaterialsConfig::PemKeyCertPair
        pem_key_cert_pair;
    auto key_materials_config =
        std::make_shared<::grpc_impl::experimental::TlsKeyMaterialsConfig>();
    ::util::Status status;
    RETURN_IF_ERROR(
        ::stratum::ReadFileToString(FLAGS_ca_cert, &pem_root_certs));
    key_materials_config->set_pem_root_certs(pem_root_certs);

    if (!FLAGS_client_cert.empty() && !FLAGS_client_key.empty()) {
      RETURN_IF_ERROR(::stratum::ReadFileToString(
          FLAGS_client_cert, &pem_key_cert_pair.cert_chain));
      RETURN_IF_ERROR(::stratum::ReadFileToString(
          FLAGS_client_key, &pem_key_cert_pair.private_key));
      key_materials_config->add_pem_key_cert_pair(pem_key_cert_pair);
    }
    auto cred_opts = ::grpc_impl::experimental::TlsCredentialsOptions(
        GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE, GRPC_TLS_SERVER_VERIFICATION,
        key_materials_config, nullptr, nullptr);

    RETURN_IF_ERROR(status);
    channel_credentials = grpc::experimental::TlsCredentials(cred_opts);
  }

  auto channel = ::grpc::CreateChannel(FLAGS_grpc_addr, channel_credentials);
  auto stub = ::p4::v1::P4Runtime::NewStub(channel);

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

  ::p4::v1::SetForwardingPipelineConfigRequest fwd_pipe_cfg_req;
  ::p4::v1::SetForwardingPipelineConfigResponse fwd_pipe_cfg_resp;
  fwd_pipe_cfg_req.set_device_id(FLAGS_device_id);
  fwd_pipe_cfg_req.mutable_election_id()->set_high(
      absl::Uint128High64(election_id));
  fwd_pipe_cfg_req.mutable_election_id()->set_low(
      absl::Uint128Low64(election_id));
  fwd_pipe_cfg_req.set_action(
      ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  RETURN_IF_ERROR(ReadProtoFromTextFile(
      FLAGS_p4info, fwd_pipe_cfg_req.mutable_config()->mutable_p4info()));
  RETURN_IF_ERROR(ReadFileToString(
      FLAGS_pipeline_cfg,
      fwd_pipe_cfg_req.mutable_config()->mutable_p4_device_config()));
  ::grpc::Status status;
  {
    ::grpc::ClientContext context;
    status = stub->SetForwardingPipelineConfig(&context, fwd_pipe_cfg_req,
                                               &fwd_pipe_cfg_resp);
    CHECK_RETURN_IF_FALSE(status.ok())
        << "Faild to send P4Runtime write request: "
        << P4RuntimeGrpcStatusToString(status);
  }

  std::string p4WriteLogs;
  RETURN_IF_ERROR(::stratum::ReadFileToString(argv[1], &p4WriteLogs));
  std::vector<std::string> lines =
      absl::StrSplit(p4WriteLogs, '\n', absl::SkipEmpty());
  ::p4::v1::WriteRequest write_req;
  ::p4::v1::WriteResponse write_resp;
  int update_prepared = 0;
  for (std::string line : lines) {
    // Log format: <timestamp>;<node_id>;<update proto>;<status>
    // TODO(Yi): Is it better to use `absl::StrSplit(line, ':')` and
    // use the third item?
    std::regex write_req_regex(";(type[^;]*);");
    std::smatch match;
    if (!std::regex_search(line, match, write_req_regex)) {
      // Can not find what we want in this line.
      VLOG(1) << "Unable to find write request message, skip: " << line;
      continue;
    }
    // The regular expression here we are using will put the protobuf text we
    // need in the first sub-match group.
    std::string write_request_text = match[1].str();
    auto update = write_req.add_updates();
    RETURN_IF_ERROR(ParseProtoFromString(write_request_text, update));
    ++update_prepared;

    if (update_prepared == FLAGS_write_batch_size) {
      update_prepared = 0;
      write_req.set_device_id(FLAGS_device_id);
      write_req.mutable_election_id()->set_high(
          absl::Uint128High64(election_id));
      write_req.mutable_election_id()->set_low(absl::Uint128Low64(election_id));
      VLOG(1) << "Sending request " << write_req.DebugString();
      ::grpc::ClientContext context;
      status = stub->Write(&context, write_req, &write_resp);
      // CHECK_RETURN_IF_FALSE(status.ok())
      //     << "Faild to send P4Runtime write request: "
      //     << P4RuntimeGrpcStatusToString(status);
      write_req.Clear();
    }
  }

  if (update_prepared != 0) {
    write_req.set_device_id(FLAGS_device_id);
    write_req.mutable_election_id()->set_high(absl::Uint128High64(election_id));
    write_req.mutable_election_id()->set_low(absl::Uint128Low64(election_id));
    VLOG(1) << "Sending request " << write_req.DebugString();
    ::grpc::ClientContext context;
    status = stub->Write(&context, write_req, &write_resp);
    // CHECK_RETURN_IF_FALSE(status.ok())
    //     << "Faild to send P4Runtime write request: "
    //     << P4RuntimeGrpcStatusToString(status);
  }

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
