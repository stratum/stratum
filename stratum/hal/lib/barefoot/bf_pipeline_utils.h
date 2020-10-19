// Copyright 2012-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTILS_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTILS_H_

#include <string>

#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/barefoot/bf.pb.h"

namespace stratum {
namespace hal {
namespace barefoot {

// Parses the P4 ForwardingPipelineConfig to extract the Barefoot pipeline.
// This method specifically extracts the pipeline from the p4_device_config
// param and supports two formats:
//     - BfPipelineConfig proto (in binary format) -- preferred
//     - archive (tar/zip) of the Barefoot compiler output
// In either case, the provided BfPipelineConfig instance is populated.
::util::Status ExtractBfPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config,
    BfPipelineConfig* bf_config);

// Converts the BfPipelineConfig instance to the legacy binary format used
// by the Barefoot PI implementation.
::util::Status BfPipelineConfigToPiConfig(const BfPipelineConfig& bf_config,
                                          std::string* pi_node_config);

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTILS_H_
