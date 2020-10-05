// Copyright 2012-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTIL_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTIL_H_

#include <string>
#include <memory>
#include <vector>

#include "p4/v1/p4runtime.pb.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/hal/lib/barefoot/bfrt.pb.h"

#include "libarchive/archive.h"
#include "libarchive/archive_entry.h"
#include "nlohmann/json.hpp"
#include "stratum/public/proto/error.pb.h"


namespace stratum {
namespace {
// Helper function to extract the contents of first file named filename from
// an in-memory archive.
::util::StatusOr<std::string> ExtractFromArchive(const std::string& archive,
                                                 const std::string& filename) {
  struct archive* a = archive_read_new();
  auto cleanup = gtl::MakeCleanup([&a]() { archive_read_free(a); });
  archive_read_support_filter_bzip2(a);
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

namespace hal {
namespace barefoot {

::util::StatusOr<BfrtDeviceConfig> ExtractBfrtDeviceConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {

  // Try a parse of BfrtDeviceConfig.
  {
    BfrtDeviceConfig device_config;
    // The pipeline config is stored as raw bytes in the p4_device_config.
    if (device_config.ParseFromString(config.p4_device_config())) {
      return device_config;
    }
  }

  // Find <prog_name>.conf file
  nlohmann::json conf;
  {
    ASSIGN_OR_RETURN(auto conf_content,
                     ExtractFromArchive(config.p4_device_config(), ".conf"));
    conf = nlohmann::json::parse(conf_content, nullptr, false);
    CHECK_RETURN_IF_FALSE(!conf.is_discarded()) << "Failed to parse .conf";
    VLOG(1) << ".conf content: " << conf.dump();
  }

  // Translate JSON conf to protobuf.
  try {
    BfrtDeviceConfig bfrt_config;
    CHECK_RETURN_IF_FALSE(conf["p4_devices"].size() == 1)
        << "Stratum only supports single devices.";
    // Only support single devices for now
    const auto& device = conf["p4_devices"][0];
    bfrt_config.set_device(device["device-id"]);
    for (const auto& program : device["p4_programs"]) {
      auto p = bfrt_config.add_programs();
      p->set_name(program["program-name"]);
      ASSIGN_OR_RETURN(
          auto bfrt_content,
          ExtractFromArchive(config.p4_device_config(), "bfrt.json"));
      p->set_bfrt(bfrt_content);
      *p->mutable_p4info() = config.p4info();
      for (const auto& pipeline : program["p4_pipelines"]) {
        auto pipe = p->add_pipelines();
        pipe->set_name(pipeline["p4_pipeline_name"]);
        for (const auto& scope : pipeline["pipe_scope"]) {
          pipe->add_scope(scope);
        }
        ASSIGN_OR_RETURN(
            const auto context_content,
            ExtractFromArchive(config.p4_device_config(),
                               absl::StrCat(pipe->name(), "/context.json")));
        pipe->set_context(context_content);
        ASSIGN_OR_RETURN(
            const auto config_content,
            ExtractFromArchive(config.p4_device_config(),
                               absl::StrCat(pipe->name(), "/tofino.bin")));
        pipe->set_config(config_content);
      }
    }
    VLOG(2) << bfrt_config.DebugString();
    return bfrt_config;
  } catch (nlohmann::json::exception& e) {
    return MAKE_ERROR(ERR_INTERNAL) << e.what();
  }

  return ::util::OkStatus(); //FIXME error
}

::util::StatusOr<string> GetLegacyPiBytes(
    const BfrtDeviceConfig& bfrt_config) {

}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif // STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTIL_H_