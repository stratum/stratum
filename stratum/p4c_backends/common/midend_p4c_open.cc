// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file contains the MidEndP4cOpen implementation, which wraps the
// p4c P4Test::MidEnd in a MidEndInterface.

#include "stratum/p4c_backends/common/midend_p4c_open.h"

#include "base/logging.h"

namespace stratum {
namespace p4c_backends {

MidEndP4cOpen::MidEndP4cOpen(CompilerOptions* p4c_options)
    : p4c_options_(ABSL_DIE_IF_NULL(p4c_options)),
      mid_end_(new P4Test::MidEnd(*p4c_options)) {}

IR::ToplevelBlock* MidEndP4cOpen::RunMidEndPass(const IR::P4Program& program) {
  if (mid_end_->toplevel != nullptr) {
    LOG(ERROR) << "The midend has already processed a P4Program";
    return nullptr;
  }

  mid_end_->addDebugHook(p4c_options_->getDebugHook());
  const IR::P4Program* p4_program = &program;
  return mid_end_->process(p4_program);
}

}  // namespace p4c_backends
}  // namespace stratum
