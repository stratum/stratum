// Copyright 2012-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/tdi/tdi_pipeline_utils.h"

#include <arpa/inet.h>

#include <string>

#include "absl/strings/strip.h"
#include "gflags/gflags.h"
#include "nlohmann/json.hpp"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace tdi {

namespace {
// Helper function to convert a uint32 to a little-endian byte string.
std::string Uint32ToLeByteStream(uint32 val) {
  uint32 tmp = (htonl(1) == (1)) ? __builtin_bswap32(val) : val;
  std::string bytes = "";
  bytes.assign(reinterpret_cast<char*>(&tmp), sizeof(uint32));
  return bytes;
}
}  // namespace

::util::Status ExtractBfPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config,
    BfPipelineConfig* bf_config) {
  bf_config->Clear();

  // p4_device_config is a serialized BfPipelineConfig proto message.
  if (bf_config->ParseFromString(config.p4_device_config())) {
    return util::OkStatus();
  }

  RETURN_ERROR(ERR_INVALID_PARAM) << "Unknown format for p4_device_config.";
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

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
