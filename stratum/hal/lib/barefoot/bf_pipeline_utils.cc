// Copyright 2012-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_pipeline_utils.h"

#include <arpa/inet.h>

#include <string>

#include "absl/strings/strip.h"
#include "gflags/gflags.h"
#include "nlohmann/json.hpp"
#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace barefoot {

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

  return MAKE_ERROR(ERR_INVALID_PARAM)
         << "Unknown format for p4_device_config.";
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
