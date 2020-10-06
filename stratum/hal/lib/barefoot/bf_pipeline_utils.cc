// Copyright 2012-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTIL_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTIL_H_

#include "stratum/hal/lib/barefoot/bf_pipeline_utils.h"

#include <arpa/inet.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/strip.h"
#include "google/protobuf/text_format.h"
#include "libarchive/archive.h"
#include "libarchive/archive_entry.h"
#include "nlohmann/json.hpp"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bf.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace {
// Helper function to see in an archive can be read
bool IsArchive(const std::string& archive) {
  struct archive* a = archive_read_new();
  auto cleanup = gtl::MakeCleanup([&a]() { archive_read_free(a); });
  archive_read_support_filter_bzip2(a);
  archive_read_support_filter_gzip(a);
  archive_read_support_filter_xz(a);
  archive_read_support_format_tar(a);
  int r = archive_read_open_memory(a, archive.c_str(), archive.size());
  return r == ARCHIVE_OK;
}

// Helper function to extract the contents of first file named filename from
// an in-memory archive.
::util::StatusOr<std::string> ExtractFromArchive(const std::string& archive,
                                                 const std::string& filename) {
  struct archive* a = archive_read_new();
  auto cleanup = gtl::MakeCleanup([&a]() { archive_read_free(a); });
  archive_read_support_filter_bzip2(a);
  archive_read_support_filter_gzip(a);
  archive_read_support_filter_xz(a);
  archive_read_support_format_tar(a);
  int r = archive_read_open_memory(a, archive.c_str(), archive.size());
  CHECK_RETURN_IF_FALSE(r == ARCHIVE_OK) << "Failed to read archive";
  struct archive_entry* entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    std::string path_name = archive_entry_pathname(entry);
    if (absl::StripSuffix(path_name, filename) != path_name) {
      VLOG(2) << "Found file: " << path_name;
      std::string content;
      content.resize(archive_entry_size(entry));
      CHECK_RETURN_IF_FALSE(archive_read_data(a, &content[0], content.size()) ==
                            content.size());
      return content;
    }
  }
  return MAKE_ERROR(ERR_ENTRY_NOT_FOUND) << "File not found: " << filename;
}
}  // namespace

namespace {
// Helper function to convert a uint32 to a little-endian byte string.
std::string Uint32ToLeByteStream(uint32 val) {
  uint32 tmp = (htonl(1) == 1) ? __builtin_bswap32(val) : val;
  std::string bytes = "";
  bytes.assign(reinterpret_cast<char*>(&tmp), sizeof(uint32));
  return bytes;
}
}  // namespace

namespace hal {
namespace barefoot {

::util::Status ExtractBfPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config,
    BfPipelineConfig* bf_config) {
  bf_config->Clear();

  // Format 1: p4_device_config is a serialized BfPipelineConfig proto message.
  if (bf_config->ParseFromString(config.p4_device_config())) {
    return util::OkStatus();
  }

  // Format 2: p4_device_config is an archive of the compiler output.
  if (IsArchive(config.p4_device_config())) {
    // Find <prog_name>.conf file.
    nlohmann::json conf;
    ASSIGN_OR_RETURN(auto conf_content,
                    ExtractFromArchive(config.p4_device_config(), ".conf"));
    conf = nlohmann::json::parse(conf_content, nullptr, false);
    CHECK_RETURN_IF_FALSE(!conf.is_discarded()) << "Failed to parse .conf";
    VLOG(1) << ".conf content: " << conf.dump();

    // Translate JSON conf to protobuf.
    try {
      CHECK_RETURN_IF_FALSE(conf["p4_devices"].size() == 1)
          << "Stratum only supports single devices.";
      // Only support single devices for now
      const auto& device = conf["p4_devices"][0];
      bf_config->set_device(device["device-id"]);
      for (const auto& program : device["p4_programs"]) {
      auto p = bf_config->add_programs();
      // name
      p->set_name(program["program-name"]);
      // bfrt.json
      ASSIGN_OR_RETURN(
          auto bfrt_content,
          ExtractFromArchive(config.p4_device_config(), "bfrt.json"));
      p->set_bfrt(bfrt_content);
      // p4info.txt
      *p->mutable_p4info() = config.p4info();
      ASSIGN_OR_RETURN(
          auto p4info_content,
          ExtractFromArchive(config.p4_device_config(), "p4info.txt"));
      ::p4::config::v1::P4Info p4info_from_tar;
      CHECK_RETURN_IF_FALSE(google::protobuf::TextFormat::ParseFromString(
          p4info_content, &p4info_from_tar)) << "Invalid p4info.txt file";
      CHECK_RETURN_IF_FALSE(ProtoEqual(p4info_from_tar, config.p4info())) <<
          "P4Info from P4 ForwardingPipelineConfig and archive do not match";
      // pipes
      for (const auto& pipeline : program["p4_pipelines"]) {
        auto pipe = p->add_pipelines();
        // pipe name
        pipe->set_name(pipeline["p4_pipeline_name"]);
        // pipe scope
        for (const auto& scope : pipeline["pipe_scope"]) {
        pipe->add_scope(scope);
        }
        // pipe context.json
        ASSIGN_OR_RETURN(
            const auto context_content,
            ExtractFromArchive(config.p4_device_config(),
                            absl::StrCat(pipe->name(), "/context.json")));
        pipe->set_context(context_content);
        // pipe tofino.bin
        ASSIGN_OR_RETURN(
            const auto config_content,
            ExtractFromArchive(config.p4_device_config(),
                            absl::StrCat(pipe->name(), "/tofino.bin")));
        pipe->set_config(config_content);
        }
      }
      VLOG(2) << bf_config->DebugString();
      return util::OkStatus();
    } catch (nlohmann::json::exception& e) {
      return MAKE_ERROR(ERR_INTERNAL) << e.what();
    }
  }

  return MAKE_ERROR(ERR_INVALID_PARAM) << "Unknown format for p4_device_config";
}

::util::Status BfPipelineConfigToPiConfig(const BfPipelineConfig& bf_config,
                                          std::string* pi_node_config) {
  CHECK_RETURN_IF_FALSE(pi_node_config) << "null pointer.";

  // Validate restrictions.
  CHECK_RETURN_IF_FALSE(bf_config.programs_size() == 1)
      << "Only single program P4 configs are supported.";
  const auto& program = bf_config.programs(0);
  CHECK_RETURN_IF_FALSE(program.pipelines_size() == 1)
      << "Only single pipeline P4 configs are supported.";
  const auto& pipeline = program.pipelines(0);

  pi_node_config->clear();
  // Program name
  pi_node_config->append(Uint32ToLeByteStream(program.name().size()));
  pi_node_config->append(program.name());
  // Tofino bin
  pi_node_config->append(Uint32ToLeByteStream(pipeline.config().size()));
  pi_node_config->append(pipeline.config());
  // Context json
  pi_node_config->append(Uint32ToLeByteStream(pipeline.context().size()));
  pi_node_config->append(pipeline.context());
  VLOG(1) << "First 16 bytes of converted PI node config: "
          << StringToHex(pi_node_config->substr(0, 16));

  return util::OkStatus();
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTIL_H_
