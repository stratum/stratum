// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

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
