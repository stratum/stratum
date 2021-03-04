// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <memory>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/security/tls_credentials_options.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/tools/benchmark/p4runtime_session.h"

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
DEFINE_string(election_id, "0,1",
              "Election id for arbitration update (high,low).");
DEFINE_uint64(device_id, 1, "P4Runtime device ID.");
DEFINE_string(ca_cert_file, "",
              "CA certificate, will use insecure credentials if empty.");
DEFINE_string(client_cert_file, "", "Client certificate (optional).");
DEFINE_string(client_key_file, "", "Client key (optional).");

namespace stratum {
namespace tools {
namespace benchmark {
namespace {

const char kUsage[] =
    R"USAGE(This tool benchmarks P4Runtime requests agains a Stratum instance.
)USAGE";

using ClientStreamChannelReaderWriter =
    ::grpc::ClientReaderWriter<::p4::v1::StreamMessageRequest,
                               ::p4::v1::StreamMessageResponse>;

std::vector<::p4::v1::TableEntry> CreateUpTo16KGenericAclTableEntries(
    int num_table_entries) {
  num_table_entries = std::min(num_table_entries, 1024 * 16);
  std::vector<::p4::v1::TableEntry> table_entries;
  table_entries.reserve(num_table_entries);
  for (int i = 0; i < num_table_entries; ++i) {
    ::p4::v1::TableEntry entry;
    std::string acl_entry_text = R"PROTO(
      table_id: 39601850
      match {
        field_id: 9
        ternary {
          value: "\000\000\000\000"
          mask: "\xff\xff\xff\xff"
        }
      }
      action {
        action {
          action_id: 21161133
        }
      }
      priority: 10
    )PROTO";
    CHECK_OK(ParseProtoFromString(acl_entry_text, &entry));
    std::string value = hal::Uint32ToByteStream(i);
    while (value.size() < 4) value.insert(0, 1, '\x00');
    CHECK_EQ(4, value.size()) << StringToHex(value) << " for i " << i;
    entry.mutable_match(0)->mutable_ternary()->set_value(value);
    table_entries.emplace_back(entry);
  }

  return table_entries;
}

::util::Status Main(int argc, char** argv) {
  ::gflags::SetUsageMessage(kUsage);
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  // Create P4Runtime session.
  auto channel_credentials = ::grpc::InsecureChannelCredentials();
  if (!FLAGS_ca_cert_file.empty()) {
    std::string ca_cert;
    RETURN_IF_ERROR(ReadFileToString(FLAGS_ca_cert_file, &ca_cert));
    std::string client_cert;
    RETURN_IF_ERROR(ReadFileToString(FLAGS_client_cert_file, &client_cert));
    std::string client_key;
    RETURN_IF_ERROR(ReadFileToString(FLAGS_client_key_file, &client_key));
    ASSIGN_OR_RETURN(
        channel_credentials,
        CreateTlsChannelCredentials(ca_cert, client_cert, client_key));
  }
  ASSIGN_OR_RETURN(auto session,
                   P4RuntimeSession::Create(
                       FLAGS_grpc_addr, channel_credentials, FLAGS_device_id));

  // Push pipeline config, if given.
  if (!FLAGS_p4_info_file.empty() && !FLAGS_p4_pipeline_config_file.empty()) {
    p4::config::v1::P4Info p4_info;
    RETURN_IF_ERROR(ReadProtoFromTextFile(FLAGS_p4_info_file, &p4_info));
    std::string p4_device_config;
    RETURN_IF_ERROR(
        ReadFileToString(FLAGS_p4_pipeline_config_file, &p4_device_config));
    RETURN_IF_ERROR(
        SetForwardingPipelineConfig(session.get(), p4_info, p4_device_config));
  }

  RETURN_IF_ERROR(ClearTableEntries(session.get()));

  {
    std::string entry_text = R"PROTO(
      table_id: 39601850
      match {
        field_id: 1
        ternary {
          value: "\001\004"
          mask: "\001\377"
        }
      }
      action {
        action {
          action_id: 21161133
        }
      }
      priority: 10
    )PROTO";
    ::p4::v1::TableEntry entry;
    RETURN_IF_ERROR(ParseProtoFromString(entry_text, &entry));
    RETURN_IF_ERROR(InstallTableEntry(session.get(), entry));
  }

  // Simple ACL bench.
  RETURN_IF_ERROR(ClearTableEntries(session.get()));
  {
    const int num_table_entries = 1023;
    auto entries = CreateUpTo16KGenericAclTableEntries(num_table_entries);
    const auto start_time = absl::Now();
    RETURN_IF_ERROR(InstallTableEntries(session.get(), entries));
    const auto end_time = absl::Now();
    absl::Duration d = end_time - start_time;
    LOG(INFO) << "Inserting " << num_table_entries << " ACL entries took " << d
              << ", " << d / num_table_entries << "/entry.";

    {
      const auto start_time = absl::Now();
      ASSIGN_OR_RETURN(auto read_entries, ReadTableEntries(session.get()));
      const auto end_time = absl::Now();
      absl::Duration d = end_time - start_time;
      LOG(INFO) << "Reading " << read_entries.size() << " ACL entries took "
                << d << ", " << d / read_entries.size() << "/entry.";
    }
  }

  LOG(INFO) << "Done.";

  return ::util::OkStatus();
}

}  // namespace
}  // namespace benchmark
}  // namespace tools
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::tools::benchmark::Main(argc, argv).error_code();
}
