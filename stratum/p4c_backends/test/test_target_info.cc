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

// The TestTargetInfo implementation is in this file.

#include "stratum/p4c_backends/test/test_target_info.h"

namespace stratum {
namespace p4c_backends {

TestTargetInfo* TestTargetInfo::test_singleton_ = nullptr;

void TestTargetInfo::SetUpTestTargetInfo() {
  test_singleton_ = new TestTargetInfo;
  TargetInfo::InjectSingleton(test_singleton_);
}

void TestTargetInfo::TearDownTestTargetInfo() {
  TargetInfo::InjectSingleton(nullptr);
  delete test_singleton_;
  test_singleton_ = nullptr;
}

bool TestTargetInfo::IsPipelineStageFixed(
    P4Annotation::PipelineStage stage) const {
  if (stage == P4Annotation::L2 || stage == P4Annotation::L3_LPM) {
    return true;
  }
  return false;
}

}  // namespace p4c_backends
}  // namespace stratum
