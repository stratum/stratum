/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This file declares a TunnelOptimizerInterface mock for unit tests.

#ifndef STRATUM_P4C_BACKENDS_FPM_TUNNEL_OPTIMIZER_MOCK_H_
#define STRATUM_P4C_BACKENDS_FPM_TUNNEL_OPTIMIZER_MOCK_H_

#include "gmock/gmock.h"
#include "stratum/p4c_backends/fpm/tunnel_optimizer_interface.h"

namespace stratum {
namespace p4c_backends {

class TunnelOptimizerMock : public TunnelOptimizerInterface {
 public:
  MOCK_METHOD2(Optimize, bool(const hal::P4ActionDescriptor& input_action,
                              hal::P4ActionDescriptor* optimized_action));
  MOCK_METHOD3(MergeAndOptimize,
               bool(const hal::P4ActionDescriptor& input_action1,
                    const hal::P4ActionDescriptor& input_action2,
                    hal::P4ActionDescriptor* optimized_action));
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_TUNNEL_OPTIMIZER_MOCK_H_
