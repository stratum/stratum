// Copyright 2012-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_pipeline_utils.h"

#include <arpa/inet.h>

#include <string>

#include "absl/strings/strip.h"
#include "gflags/gflags.h"
#include "libarchive/archive.h"
#include "libarchive/archive_entry.h"
#include "nlohmann/json.hpp"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

DEFINE_bool(incompatible_enable_p4_device_config_tar, false,
            "Enables support for p4_device_config as a tarball.");

namespace stratum {
namespace hal {
namespace barefoot {

namespace {
// Helper function to check if a binary string is a valid archive.
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
  RETURN_ERROR(ERR_ENTRY_NOT_FOUND) << "File not found: " << filename;
}

// Helper function to convert a uint32 to a little-endian byte string.
std::string Uint32ToLeByteStream(uint32 val) {
  uint32 tmp = (htonl(1) == 1) ? __builtin_bswap32(val) : val;
  std::string bytes = "";
  bytes.assign(reinterpret_cast<char*>(&tmp), sizeof(uint32));
  return bytes;
}
}  // namespace

::util::Status ExtractBfPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config,
    BfPipelineConfig* bf_config) {
  bf_config->Clear();

  // Format 1: p4_device_config is a serialized BfPipelineConfig proto message.
  if (bf_config->ParseFromString(config.p4_device_config())) {
    return util::OkStatus();
  }

  // Format 2: p4_device_config is an archive of the compiler output.
  if (FLAGS_incompatible_enable_p4_device_config_tar &&
      IsArchive(config.p4_device_config())) {
    // Find <prog_name>.conf file.
    nlohmann::json conf;
    ASSIGN_OR_RETURN(auto conf_content,
                     ExtractFromArchive(config.p4_device_config(), ".conf"));
    conf = nlohmann::json::parse(conf_content, nullptr, false);
    CHECK_RETURN_IF_FALSE(!conf.is_discarded()) << "Failed to parse .conf";
    VLOG(2) << ".conf content: " << conf.dump();

    // Translate JSON conf to protobuf.
    try {
      CHECK_RETURN_IF_FALSE(conf["p4_devices"].size() == 1)
          << "Stratum only supports single devices.";
      // Only support single devices for now.
      const auto& device = conf["p4_devices"][0];
      for (const auto& program : device["p4_programs"]) {
        // p4 name
        bf_config->set_p4_name(program["program-name"]);
        // bfrt.json
        ASSIGN_OR_RETURN(
            auto bfrt_content,
            ExtractFromArchive(config.p4_device_config(), "bfrt.json"));
        bf_config->set_bfruntime_info(bfrt_content);
        // pipes
        for (const auto& pipeline : program["p4_pipelines"]) {
          auto profile = bf_config->add_profiles();
          // profile name
          profile->set_profile_name(pipeline["p4_pipeline_name"]);
          // profile scope
          for (const auto& scope : pipeline["pipe_scope"]) {
            profile->add_pipe_scope(scope);
          }
          // profile context.json
          ASSIGN_OR_RETURN(
              const auto context_content,
              ExtractFromArchive(
                  config.p4_device_config(),
                  absl::StrCat(profile->profile_name(), "/context.json")));
          profile->set_context(context_content);
          // profile tofino.bin
          ASSIGN_OR_RETURN(
              const auto config_content,
              ExtractFromArchive(
                  config.p4_device_config(),
                  absl::StrCat(profile->profile_name(), "/tofino.bin")));
          profile->set_binary(config_content);
        }
      }
      VLOG(2) << bf_config->DebugString();
      return util::OkStatus();
    } catch (nlohmann::json::exception& e) {
      RETURN_ERROR(ERR_INTERNAL) << e.what();
    }
  }

  RETURN_ERROR(ERR_INVALID_PARAM) << "Unknown format for p4_device_config";
}

::util::Status BfPipelineConfigToPiConfig(const BfPipelineConfig& bf_config,
                                          std::string* pi_node_config) {
  CHECK_RETURN_IF_FALSE(pi_node_config) << "null pointer.";

  // Validate restrictions.
  CHECK_RETURN_IF_FALSE(bf_config.profiles_size() == 1)
      << "Only single pipeline P4 configs are supported.";
  const auto& profile = bf_config.profiles(0);

  pi_node_config->clear();
  // Program name
  pi_node_config->append(Uint32ToLeByteStream(bf_config.p4_name().size()));
  pi_node_config->append(bf_config.p4_name());
  // Tofino bin
  pi_node_config->append(Uint32ToLeByteStream(profile.binary().size()));
  pi_node_config->append(profile.binary());
  // Context json
  pi_node_config->append(Uint32ToLeByteStream(profile.context().size()));
  pi_node_config->append(profile.context());
  VLOG(2) << "First 16 bytes of converted PI node config: "
          << StringToHex(pi_node_config->substr(0, 16));

  return util::OkStatus();
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
