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

// Converts the BfPipelineConfig instance to the legacy binary format used
// by the Barefoot PI implementation.
::util::Status BfPipelineConfigToPiConfig(const BfPipelineConfig& bf_config,
                                          std::string* pi_node_config);

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PIPELINE_UTILS_H_
