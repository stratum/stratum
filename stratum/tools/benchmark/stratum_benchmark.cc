// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <memory>

#include "absl/random/random.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
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
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/p4runtime/entity_management.h"
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
DEFINE_string(election_id, "0,1",
              "Election id for arbitration update (high,low).");
DEFINE_uint64(device_id, 1, "P4Runtime device ID.");
DEFINE_string(ca_cert_file, "",
              "CA certificate, will use insecure credentials if empty.");
DEFINE_string(client_cert_file, "", "Client certificate (optional).");
DEFINE_string(client_key_file, "", "Client key (optional).");
DEFINE_bool(robot, false, "Whether to print results in machine-readable form.");

namespace stratum {
namespace tools {
namespace benchmark {
namespace {

using p4runtime::BuildP4RTEntityIdReplacementMap;
using p4runtime::CreateTlsChannelCredentials;
using p4runtime::HydrateP4RuntimeProtoFromStringOrDie;

const char kUsage[] =
    R"USAGE(This tool benchmarks P4Runtime requests agains a Stratum instance.
)USAGE";

std::string BytestringToPaddedBytestring(std::string bytestring,
                                         int bit_width) {
  int num_bytes = (bit_width + 7) / 8;
  while (bytestring.size() < num_bytes) {
    bytestring.insert(0, 1, '\x00');
  }

  return bytestring;
}

std::string FormatBenchTime(absl::Duration d, int ops) {
  if (FLAGS_robot) {
    return absl::StrCat(
        ops, ", ",
        absl::ToDoubleMilliseconds(absl::Trunc(d, absl::Microseconds(100))),
        ", ",
        absl::ToDoubleMicroseconds(absl::Trunc(d / ops, absl::Microseconds(1))),
        ", ", ops / absl::ToDoubleSeconds(d));
  } else {
    return absl::StrCat(
        ops, " ops, ",
        absl::FormatDuration(absl::Trunc(d, absl::Microseconds(100))), ", ",
        absl::FormatDuration(absl::Trunc(d / ops, absl::Microseconds(1))),
        "/op, ", ops / absl::ToDoubleSeconds(d), " ops/s");
  }
}

// Table sizes are not exact on Tofino and depend on many factors, some even at
// runtime. Therefore, we define a minimum fill level that must reachable.
int InsertFailureDisallowedFillLevel(int table_size) {
  return table_size * 0.8;  // 80%
}

class FabricBenchmark {
 public:
  FabricBenchmark() {}

  ::util::Status Init() {
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
    ASSIGN_OR_RETURN(
        session_, p4runtime::P4RuntimeSession::Create(
                      FLAGS_grpc_addr, channel_credentials, FLAGS_device_id));

    // Push pipeline config if given, else read from switch.
    if (!FLAGS_p4_info_file.empty() && !FLAGS_p4_pipeline_config_file.empty()) {
      RETURN_IF_ERROR(ReadProtoFromTextFile(FLAGS_p4_info_file, &p4_info_));
      std::string p4_device_config;
      RETURN_IF_ERROR(
          ReadFileToString(FLAGS_p4_pipeline_config_file, &p4_device_config));
      RETURN_IF_ERROR(SetForwardingPipelineConfig(session_.get(), p4_info_,
                                                  p4_device_config));
    } else {
      std::string p4_device_config;
      RETURN_IF_ERROR(GetForwardingPipelineConfig(session_.get(), &p4_info_,
                                                  &p4_device_config));
    }

    // Resolve commonly used P4 objects.
    RETURN_IF_ERROR(
        BuildP4RTEntityIdReplacementMap(p4_info_, &p4_id_replacements_));
    p4_info_manager_ = absl::make_unique<hal::P4InfoManager>(p4_info_);
    RETURN_IF_ERROR(p4_info_manager_->InitializeAndVerify());
    ASSIGN_OR_RETURN(
        acl_table_, p4_info_manager_->FindTableByName("FabricIngress.acl.acl"));
    ASSIGN_OR_RETURN(far_table_, p4_info_manager_->FindTableByName(
                                     "FabricIngress.spgw.fars"));
    ASSIGN_OR_RETURN(uplink_pdr_table_, p4_info_manager_->FindTableByName(
                                            "FabricIngress.spgw.uplink_pdrs"));
    ASSIGN_OR_RETURN(
        downlink_pdr_table_,
        p4_info_manager_->FindTableByName("FabricIngress.spgw.downlink_pdrs"));
    ASSIGN_OR_RETURN(
        ingress_pdr_counter_,
        p4_info_manager_->FindCounterByName("FabricIngress.spgw.pdr_counter"));
    ASSIGN_OR_RETURN(load_normal_far_action_,
                     p4_info_manager_->FindActionByName(
                         "FabricIngress.spgw.load_normal_far"));
    ASSIGN_OR_RETURN(load_tunnel_far_action_,
                     p4_info_manager_->FindActionByName(
                         "FabricIngress.spgw.load_tunnel_far"));
    ASSIGN_OR_RETURN(
        load_dbuf_far_action_,
        p4_info_manager_->FindActionByName("FabricIngress.spgw.load_dbuf_far"));

    RETURN_IF_ERROR(ClearTableEntries(session_.get()));

    return ::util::OkStatus();
  }

  ::util::Status DoInsertBenchmark(
      std::string table_name,
      const std::vector<::p4::v1::TableEntry>& entries) {
    const auto start_time = absl::Now();
    RETURN_IF_ERROR(InstallTableEntries(session_.get(), entries));
    const auto end_time = absl::Now();
    absl::Duration d = end_time - start_time;
    LOG(INFO) << "Inserting " << table_name
              << " entries: " << FormatBenchTime(d, entries.size()) << ".";

    return ::util::OkStatus();
  }

  ::util::Status DoDeleteBenchmark(
      std::string table_name,
      const std::vector<::p4::v1::TableEntry>& entries) {
    const auto start_time = absl::Now();
    RETURN_IF_ERROR(RemoveTableEntries(session_.get(), entries));
    const auto end_time = absl::Now();
    absl::Duration d = end_time - start_time;
    LOG(INFO) << "Deleting " << table_name
              << " entries: " << FormatBenchTime(d, entries.size()) << ".";

    return ::util::OkStatus();
  }

  ::util::Status DoReadBenchmark(std::string table_name,
                                 bool include_counter_data = false) {
    const auto start_time = absl::Now();
    ASSIGN_OR_RETURN(
        auto read_entries,
        ReadTableEntries(session_.get(), include_counter_data, false));
    const auto end_time = absl::Now();
    absl::Duration d = end_time - start_time;
    LOG(INFO) << "Reading " << table_name << " entries "
              << (include_counter_data ? "including counters" : "") << ": "
              << FormatBenchTime(d, read_entries.size()) << ".";

    return ::util::OkStatus();
  }

  ::util::Status DoCounterModifyBenchmark(
      std::string table_name,
      const std::vector<::p4::v1::CounterEntry>& entries) {
    const auto start_time = absl::Now();
    RETURN_IF_ERROR(ModifyIndirectCounterEntries(session_.get(), entries));
    const auto end_time = absl::Now();
    absl::Duration d = end_time - start_time;
    LOG(INFO) << "Modifying " << table_name
              << " counters: " << FormatBenchTime(d, entries.size()) << ".";

    return ::util::OkStatus();
  }

  ::util::Status DoCounterReadBenchmark(std::string counter_name,
                                        int counter_id) {
    const auto start_time = absl::Now();
    ASSIGN_OR_RETURN(auto read_entries,
                     ReadCounterEntries(session_.get(), counter_id));
    const auto end_time = absl::Now();
    absl::Duration d = end_time - start_time;
    LOG(INFO) << "Reading " << counter_name
              << " counters: " << FormatBenchTime(d, read_entries.size())
              << ".";

    return ::util::OkStatus();
  }

  ::util::Status RunFullSwitchReadBenchmark() {
    RETURN_IF_ERROR(ClearTableEntries(session_.get()));
    RETURN_IF_ERROR(InstallTableEntries(
        session_.get(),
        CreateUpTo16KGenericAclTableEntries(
            InsertFailureDisallowedFillLevel(acl_table_.size()))));
    RETURN_IF_ERROR(InstallTableEntries(
        session_.get(),
        CreateUpTo16KGenericFarTableEntries(
            InsertFailureDisallowedFillLevel(far_table_.size()))));
    RETURN_IF_ERROR(InstallTableEntries(
        session_.get(),
        CreateUpTo16KGenericUplinkPdrTableEntries(
            InsertFailureDisallowedFillLevel(uplink_pdr_table_.size()))));
    RETURN_IF_ERROR(DoReadBenchmark("all tables"));

    return ::util::OkStatus();
  }

  ::util::Status RunAclBenchmark(bool with_counters = false) {
    RETURN_IF_ERROR(ClearTableEntries(session_.get()));
    const int num_table_entries =
        InsertFailureDisallowedFillLevel(acl_table_.size());
    const std::string table_name = acl_table_.preamble().name();
    auto entries = CreateUpTo16KGenericAclTableEntries(num_table_entries);
    RETURN_IF_ERROR(DoInsertBenchmark(table_name, entries));
    RETURN_IF_ERROR(DoReadBenchmark(table_name, false));
    RETURN_IF_ERROR(DoReadBenchmark(table_name, true));
    RETURN_IF_ERROR(DoDeleteBenchmark(table_name, entries));

    return ::util::OkStatus();
  }

  ::util::Status RunFarBenchmark() {
    RETURN_IF_ERROR(ClearTableEntries(session_.get()));
    const int num_far_entries =
        InsertFailureDisallowedFillLevel(far_table_.size());
    const std::string table_name = far_table_.preamble().name();
    auto entries = CreateUpTo16KGenericFarTableEntries(num_far_entries);
    RETURN_IF_ERROR(DoInsertBenchmark(table_name, entries));
    RETURN_IF_ERROR(DoReadBenchmark(table_name));
    RETURN_IF_ERROR(DoDeleteBenchmark(table_name, entries));

    return ::util::OkStatus();
  }

  ::util::Status RunPdrBenchmark() {
    RETURN_IF_ERROR(ClearTableEntries(session_.get()));
    const int num_pdr_entries =
        InsertFailureDisallowedFillLevel(uplink_pdr_table_.size());
    const std::string table_name = uplink_pdr_table_.preamble().name();
    auto entries = CreateUpTo16KGenericUplinkPdrTableEntries(num_pdr_entries);
    RETURN_IF_ERROR(DoInsertBenchmark(table_name, entries));
    RETURN_IF_ERROR(DoReadBenchmark(table_name));
    RETURN_IF_ERROR(DoDeleteBenchmark(table_name, entries));

    return ::util::OkStatus();
  }

  ::util::Status RunPdrCounterBenchmark() {
    RETURN_IF_ERROR(ClearTableEntries(session_.get()));
    const int num_pdr_counters = (ingress_pdr_counter_.size());
    const std::string table_name = ingress_pdr_counter_.preamble().name();
    const int counter_id = ingress_pdr_counter_.preamble().id();
    auto entries =
        CreateUpTo32KGenericIngressPdrCounterEntries(num_pdr_counters);
    RETURN_IF_ERROR(DoCounterModifyBenchmark(table_name, entries));
    RETURN_IF_ERROR(DoCounterReadBenchmark(table_name, counter_id));

    return ::util::OkStatus();
  }

  // Generate interesting batch sizes up the maximum.
  std::vector<int> SampleBatchSizes(int max_batch_size) {
    std::vector<int> batch_sizes;
    for (int i = 0, j = 1; i < 16; ++i, j = 1 << i) {
      if (max_batch_size > j) {
        batch_sizes.push_back(j);
      }
    }
    batch_sizes.push_back(max_batch_size);

    return batch_sizes;
  }

  ::util::Status RunGtpTunnelBenchmark() {
    // const int num_tunnels = 5120;
    // const int num_tunnels = 1024;
    const int num_tunnels =
        InsertFailureDisallowedFillLevel(uplink_pdr_table_.size());

    RETURN_IF_ERROR(ClearTableEntries(session_.get()));

    std::vector<std::vector<::p4::v1::TableEntry>> tunnels =
        CreateGtpTunnelsTableEntries(num_tunnels);
    const int flows_per_tunnel = tunnels[0].size();
    const int total_num_flows = tunnels.size() * flows_per_tunnel;

    std::function<std::string(std::string, int, int)> PrintBenchHeader =
        [](std::string table_op, int batch_size, int flows_per_tunnel) {
          return absl::StrFormat(
              "Batched tunnel %s (batch size %4i, %i flows/tunnel): ", table_op,
              batch_size, flows_per_tunnel);
        };
    if (FLAGS_robot) {
      LOG(INFO) << "op, batch_size, flow_per_tunnel, #ops, total_time_ms, "
                   "us_per_op, ops_per_second";
      PrintBenchHeader = [](std::string table_op, int batch_size,
                            int flows_per_tunnel) {
        return absl::StrFormat("batched_%s, %i, %i, ", table_op, batch_size,
                               flows_per_tunnel);
      };
    }

    // Variable-sized batch inserts.
    for (const auto& tunnels_per_batch : SampleBatchSizes(num_tunnels)) {
      RETURN_IF_ERROR(ClearTableEntries(session_.get()));
      // Create batches.
      std::vector<std::vector<::p4::v1::TableEntry>> sized_batches;
      for (auto it = tunnels.begin(); it != tunnels.end();) {
        std::vector<::p4::v1::TableEntry> new_batch;
        for (int i = 0; i < tunnels_per_batch && it != tunnels.end();
             ++i, ++it) {
          new_batch.insert(new_batch.end(), it->begin(), it->end());
        }
        if ((new_batch.size() / flows_per_tunnel) != tunnels_per_batch) {
          LOG(WARNING) << "Partial batch of size "
                       << new_batch.size() / flows_per_tunnel << ", wanted "
                       << tunnels_per_batch << ".";
        }
        sized_batches.push_back(new_batch);
      }
      // Mind the last partial batch.
      // CHECK_EQ(sized_batches.size(),
      //          (tunnels.size() + tunnels_per_batch - 1) / tunnels_per_batch);

      // Measure inserts.
      {
        const auto start_time = absl::Now();
        for (const auto& entries : sized_batches) {
          RETURN_IF_ERROR(InstallTableEntries(session_.get(), entries));
        }
        const auto end_time = absl::Now();
        absl::Duration d = end_time - start_time;
        LOG(INFO) << PrintBenchHeader("insert", tunnels_per_batch,
                                      tunnels[0].size())
                  << FormatBenchTime(d, tunnels.size());
        ASSIGN_OR_RETURN(auto read_entries, ReadTableEntries(session_.get()));
        CHECK_EQ(read_entries.size(), total_num_flows);
      }
      // Measure modifies.
      if ((1)) {
        // TODO(max): change dst ip
        for (const auto& entries : sized_batches) {
        }
        const auto start_time = absl::Now();
        for (const auto& entries : sized_batches) {
          RETURN_IF_ERROR(ModifyTableEntries(session_.get(), entries));
        }
        const auto end_time = absl::Now();
        absl::Duration d = end_time - start_time;
        LOG(INFO) << PrintBenchHeader("modify", tunnels_per_batch,
                                      tunnels[0].size())
                  << FormatBenchTime(d, tunnels.size());
        ASSIGN_OR_RETURN(auto read_entries, ReadTableEntries(session_.get()));
        CHECK_EQ(read_entries.size(), total_num_flows);
      }
      // Measure deletes.
      if ((1)) {
        const auto start_time = absl::Now();
        for (const auto& entries : sized_batches) {
          RETURN_IF_ERROR(RemoveTableEntries(session_.get(), entries));
        }
        const auto end_time = absl::Now();
        absl::Duration d = end_time - start_time;
        LOG(INFO) << PrintBenchHeader("delete", tunnels_per_batch,
                                      tunnels[0].size())
                  << FormatBenchTime(d, tunnels.size());
        ASSIGN_OR_RETURN(auto read_entries, ReadTableEntries(session_.get()));
        CHECK_EQ(read_entries.size(), 0);
      }
    }

    return ::util::OkStatus();
  }

  std::vector<std::vector<::p4::v1::TableEntry>> CreateGtpTunnelsTableEntries(
      int num_tunnels) {
    num_tunnels = std::min(num_tunnels, 1024 * 16);
    absl::BitGen bitgen_;
    absl::BitGen ue_bitgen;
    std::vector<std::vector<::p4::v1::TableEntry>> tunnels;
    tunnels.reserve(num_tunnels);

    for (int i = 0; i < num_tunnels; ++i) {
      std::vector<::p4::v1::TableEntry> table_entries;

      // We expect between 1-10 stable uplink destinations accross all tunnels.
      std::string uplink_ipv4_dst = BytestringToPaddedBytestring(
          hal::Uint32ToByteStream(absl::Uniform(bitgen_, 0u, kuint32max)), 32);
      // We expect all UEs to be in the same 1-10 subnets.
      std::string ue_ipv4_dst = BytestringToPaddedBytestring(
          hal::Uint32ToByteStream(
              absl::Uniform(ue_bitgen, 0x10000000u, 0x14ffffffu)),
          32);
      // Random teid per tunnel.
      uint32 teid = absl::Uniform(bitgen_, 0u, kuint32max);
      uint16 uplink_counter_id = absl::Uniform(bitgen_, 0, kuint16max);
      uint16 downlink_counter_id = absl::Uniform(bitgen_, 0, kuint16max);
      uint32 uplink_far_id = absl::Uniform(bitgen_, 0u, kuint32max);
      uint32 downlink_far_id = absl::Uniform(bitgen_, 0u, kuint32max);

      // Uplink PDR (packet detection rule)
      table_entries.emplace_back(CreateGenericUplinkPdrEntry(
          uplink_ipv4_dst, teid, uplink_counter_id, uplink_far_id));
      // Downlink PDR (packet detection rule)
      table_entries.emplace_back(CreateDownlinkPdrEntry(
          ue_ipv4_dst, downlink_counter_id, downlink_far_id));
      // Uplink tunnel FAR (forwarding action rule)
      table_entries.emplace_back(CreateTunnelFarEntry(uplink_far_id));
      // Downlink tunnel FAR (forwarding action rule)
      table_entries.emplace_back(
          CreateTunnelFarEntry(downlink_far_id, 1234, "", uplink_ipv4_dst));

      tunnels.push_back(table_entries);
    }

    return tunnels;
  }

  std::vector<::p4::v1::TableEntry> CreateUpTo16KGenericAclTableEntries(
      int num_table_entries) {
    num_table_entries = std::min(num_table_entries, 1024 * 16);
    std::vector<::p4::v1::TableEntry> table_entries;
    table_entries.reserve(num_table_entries);
    ::p4::v1::TableEntry acl_entry =
        HydrateP4RuntimeProtoFromStringOrDie<::p4::v1::TableEntry>(
            p4_id_replacements_,
            // p4_info_,
            R"PROTO(
        table_id: {FabricIngress.acl.acl}
        match {
          field_id: {FabricIngress.acl.acl.ipv4_src}
          ternary {
            value: "\000\000\000\000"
            mask: "\xff\xff\xff\xff"
          }
        }
        action {
          action {
            action_id: {FabricIngress.acl.copy_to_cpu}
          }
        }
        priority: 10
      )PROTO");
    for (int i = 0; i < num_table_entries; ++i) {
      ::p4::v1::TableEntry entry = acl_entry;
      std::string value = hal::Uint32ToByteStream(i);
      while (value.size() < 4) value.insert(0, 1, '\x00');
      CHECK_EQ(4, value.size()) << StringToHex(value) << " for i " << i;
      entry.mutable_match(0)->mutable_ternary()->set_value(value);
      table_entries.emplace_back(entry);
    }

    return table_entries;
  }

  std::vector<::p4::v1::TableEntry> CreateUpTo16KGenericFarTableEntries(
      int num_table_entries) {
    num_table_entries = std::min(num_table_entries, 1024 * 16);
    std::vector<::p4::v1::TableEntry> table_entries;
    table_entries.reserve(num_table_entries);
    for (int i = 0; i < num_table_entries; ++i) {
      ::p4::v1::TableEntry entry =
          CreateTunnelFarEntry(absl::Uniform(bitgen_, 0u, kuint32max));
      table_entries.emplace_back(entry);
    }

    return table_entries;
  }

  std::vector<::p4::v1::TableEntry> CreateUpTo16KGenericUplinkPdrTableEntries(
      int num_table_entries) {
    num_table_entries = std::min(num_table_entries, 1024 * 16);
    std::vector<::p4::v1::TableEntry> table_entries;
    table_entries.reserve(num_table_entries);
    for (int i = 0; i < num_table_entries; ++i) {
      ::p4::v1::TableEntry entry = CreateGenericUplinkPdrEntry();
      std::string uplink_ipv4_dst = BytestringToPaddedBytestring(
          hal::Uint32ToByteStream(absl::Uniform(bitgen_, 0u, kuint32max)), 32);
      std::string uplink_counter_id = BytestringToPaddedBytestring(
          hal::Uint32ToByteStream(absl::Uniform(bitgen_, 0, kuint16max)), 16);
      std::string teid = BytestringToPaddedBytestring(
          hal::Uint32ToByteStream(absl::Uniform(bitgen_, 0u, kuint32max)), 32);
      std::string uplink_far_id = BytestringToPaddedBytestring(
          hal::Uint32ToByteStream(absl::Uniform(bitgen_, 0u, kuint32max)), 32);
      entry.mutable_match(0)->mutable_exact()->set_value(uplink_ipv4_dst);
      entry.mutable_match(1)->mutable_exact()->set_value(teid);
      entry.mutable_action()->mutable_action()->mutable_params(0)->set_value(
          uplink_counter_id);
      entry.mutable_action()->mutable_action()->mutable_params(1)->set_value(
          uplink_far_id);

      table_entries.emplace_back(entry);
    }

    return table_entries;
  }

  std::vector<::p4::v1::CounterEntry>
  CreateUpTo32KGenericIngressPdrCounterEntries(int num_counter_entries) {
    num_counter_entries = std::min(num_counter_entries, 32 * 1024);
    std::vector<::p4::v1::CounterEntry> counter_entries;
    counter_entries.reserve(num_counter_entries);
    for (int i = 0; i < num_counter_entries; ++i) {
      ::p4::v1::CounterEntry entry = CreateGenericIngressPdrCounter();
      entry.mutable_index()->set_index(i);
      entry.mutable_data()->set_packet_count(
          absl::Uniform(bitgen_, 0, kuint16max));
      entry.mutable_data()->set_byte_count(
          absl::Uniform(bitgen_, 0, kuint16max));

      counter_entries.emplace_back(entry);
    }

    return counter_entries;
  }

 private:
  ::util::StatusOr<const ::p4::config::v1::Action::Param> FindActionParamByName(
      std::string action_name, std::string param_name) {
    ASSIGN_OR_RETURN(auto action,
                     p4_info_manager_->FindActionByName(action_name));
    for (const auto& param : action.params()) {
      if (param.name() == param_name) {
        return param;
      }
    }

    RETURN_ERROR(ERR_ENTRY_NOT_FOUND)
        << "Action parameter " << param_name << " not found in action "
        << action_name << ".";
  }

  ::p4::v1::TableEntry CreateTunnelFarEntry(
      uint32 far_id = 1, uint16 tunnel_src_port = 1234,
      std::string tunnel_src_addr = std::string("\x0a\x00\x00\x01", 4),
      std::string tunnel_dst_addr = std::string("\x0a\x00\x00\x02", 4)) {
    ::p4::v1::TableEntry far_entry =
        HydrateP4RuntimeProtoFromStringOrDie<::p4::v1::TableEntry>(
            p4_id_replacements_,
            R"PROTO(
          table_id: {FabricIngress.spgw.fars}
          match {
            field_id: {FabricIngress.spgw.fars.far_id}
            exact {
              value: "\x00"
            }
          }
          action {
            action {
              action_id: {FabricIngress.spgw.load_tunnel_far}
              params {
                param_id: {FabricIngress.spgw.load_tunnel_far.drop}
                value: "\x00"
              }
              params {
                param_id: {FabricIngress.spgw.load_tunnel_far.notify_cp}
                value: "\x00"
              }
              params {
                param_id: {FabricIngress.spgw.load_tunnel_far.tunnel_src_port}
                value: "\x00"
              }
              params {
                param_id: {FabricIngress.spgw.load_tunnel_far.tunnel_src_addr}
                value: "\x00"
              }
              params {
                param_id: {FabricIngress.spgw.load_tunnel_far.tunnel_dst_addr}
                value: "\x00"
              }
            }
          }
        )PROTO");

    far_entry.mutable_match(0)->mutable_exact()->set_value(
        BytestringToPaddedBytestring(hal::Uint32ToByteStream(far_id), 32));
    far_entry.mutable_action()->mutable_action()->mutable_params(2)->set_value(
        hal::Uint32ToByteStream(tunnel_src_port));
    far_entry.mutable_action()->mutable_action()->mutable_params(3)->set_value(
        tunnel_src_addr);
    far_entry.mutable_action()->mutable_action()->mutable_params(4)->set_value(
        tunnel_dst_addr);

    return far_entry;
  }

  ::p4::v1::TableEntry CreateGenericUplinkPdrEntry(
      std::string tunnel_dst_addr = std::string("\x0a\x00\x00\x01", 4),
      uint32 teid = 1, uint16 counter_id = 1, uint32 far_id = 1) {
    const std::string uplink_pdr_entry_text = R"PROTO(
      table_id: 41289867 # FabricIngress.spgw.uplink_pdrs
      match {
        field_id: 1 # tunnel_ipv4_dst
        exact {
          value: "\000\000\000\000"
        }
      }
      match {
        field_id: 2 # teid
        exact {
          value: "\000\000\000\000"
        }
      }
      action {
        action {
          action_id: 18504550 # FabricIngress.spgw.load_pdr
          params {
            param_id: 1 # ctr_id
            value: "\x00"
          }
          params {
            param_id: 2 # far_id
            value: "\x00"
          }
          params {
            param_id: 3 # needs_gtpu_decap
            value: "\x00"
          }
        }
      }
    )PROTO";
    ::p4::v1::TableEntry uplink_pdr;
    CHECK_OK(ParseProtoFromString(uplink_pdr_entry_text, &uplink_pdr));
    uplink_pdr.mutable_match(0)->mutable_exact()->set_value(
        BytestringToPaddedBytestring(tunnel_dst_addr, 32));
    uplink_pdr.mutable_match(1)->mutable_exact()->set_value(
        BytestringToPaddedBytestring(hal::Uint32ToByteStream(teid), 32));
    uplink_pdr.mutable_action()->mutable_action()->mutable_params(0)->set_value(
        BytestringToPaddedBytestring(hal::Uint32ToByteStream(counter_id), 16));
    uplink_pdr.mutable_action()->mutable_action()->mutable_params(1)->set_value(
        BytestringToPaddedBytestring(hal::Uint32ToByteStream(far_id), 32));

    return uplink_pdr;
  }

  ::p4::v1::TableEntry CreateDownlinkPdrEntry(
      std::string ue_addr = std::string("\x0a\x00\x00\x01", 4),
      uint16 counter_id = 1, uint32 far_id = 1) {
    const std::string downlink_pdr_entry_text = R"PROTO(
      table_id: 47761714 # FabricIngress.spgw.downlink_pdrs
      match {
        field_id: 1 # ue_addr
        exact {
          value: "\000\000\000\000"
        }
      }
      action {
        action {
          action_id: 18504550 # FabricIngress.spgw.load_pdr
          params {
            param_id: 1 # ctr_id
            value: "\x00"
          }
          params {
            param_id: 2 # far_id
            value: "\x00"
          }
          params {
            param_id: 3 # needs_gtpu_decap
            value: "\x00"
          }
        }
      }
    )PROTO";
    ::p4::v1::TableEntry downlink_pdr;
    CHECK_OK(ParseProtoFromString(downlink_pdr_entry_text, &downlink_pdr));
    downlink_pdr.mutable_match(0)->mutable_exact()->set_value(
        BytestringToPaddedBytestring(ue_addr, 32));
    downlink_pdr.mutable_action()
        ->mutable_action()
        ->mutable_params(0)
        ->set_value(BytestringToPaddedBytestring(
            hal::Uint32ToByteStream(counter_id), 16));
    downlink_pdr.mutable_action()
        ->mutable_action()
        ->mutable_params(1)
        ->set_value(
            BytestringToPaddedBytestring(hal::Uint32ToByteStream(far_id), 32));

    return downlink_pdr;
  }

  ::p4::v1::CounterEntry CreateGenericIngressPdrCounter() {
    const std::string ingress_pdr_counter_text = R"PROTO(
      counter_id: 0
      index {
        index: 0
      }
      data {
        packet_count: 0
        byte_count: 0
      }
    )PROTO";
    ::p4::v1::CounterEntry ingress_pdr;
    CHECK_OK(ParseProtoFromString(ingress_pdr_counter_text, &ingress_pdr));
    ingress_pdr.set_counter_id(ingress_pdr_counter_.preamble().id());

    return ingress_pdr;
  }

  // Shared random generator.
  absl::BitGen bitgen_;

  // References to Fabric TNA tables.
  p4::config::v1::Table acl_table_;
  p4::config::v1::Table far_table_;
  p4::config::v1::Table uplink_pdr_table_;
  p4::config::v1::Table downlink_pdr_table_;
  p4::config::v1::Action load_normal_far_action_;
  p4::config::v1::Action load_tunnel_far_action_;
  p4::config::v1::Action load_dbuf_far_action_;
  p4::config::v1::Counter ingress_pdr_counter_;

  // General P4RT objects.
  std::unique_ptr<p4runtime::P4RuntimeSession> session_;
  p4::config::v1::P4Info p4_info_;
  std::unique_ptr<hal::P4InfoManager> p4_info_manager_;
  absl::flat_hash_map<std::string, std::string> p4_id_replacements_;
};

::util::Status Main(int argc, char** argv) {
  ::gflags::SetUsageMessage(kUsage);
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  FabricBenchmark benchmark;
  RETURN_IF_ERROR(benchmark.Init());

  RETURN_IF_ERROR(benchmark.RunAclBenchmark());
  RETURN_IF_ERROR(benchmark.RunFarBenchmark());
  RETURN_IF_ERROR(benchmark.RunPdrBenchmark());
  RETURN_IF_ERROR(benchmark.RunPdrCounterBenchmark());
  RETURN_IF_ERROR(benchmark.RunFullSwitchReadBenchmark());
  RETURN_IF_ERROR(benchmark.RunGtpTunnelBenchmark());

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
