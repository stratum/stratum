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

// TestTargetInfo is a TargetInfo subclass for unit test use.  It implements
// general behavior suitable for tests that don't need to use the
// TargetInfoMock class to define specific TargetInfo expectations.

#ifndef STRATUM_P4C_BACKENDS_TEST_TEST_TARGET_INFO_H_
#define STRATUM_P4C_BACKENDS_TEST_TEST_TARGET_INFO_H_

#include "stratum/p4c_backends/fpm/target_info.h"

namespace stratum {
namespace p4c_backends {

class TestTargetInfo : public TargetInfo {
 public:
  ~TestTargetInfo() override {}

  // These two static methods manage a singleton TestTargetInfo instance
  // for unit tests.
  static void SetUpTestTargetInfo();
  static void TearDownTestTargetInfo();

  // This override returns true for the L2 and L3_LPM stages and false
  // for all other stages.
  bool IsPipelineStageFixed(P4Annotation::PipelineStage stage) const override;

  // TestTargetInfo is neither copyable nor movable.
  TestTargetInfo(const TestTargetInfo&) = delete;
  TestTargetInfo& operator=(const TestTargetInfo&) = delete;

 private:
  TestTargetInfo() {}  // Use SetUpTestTargetInfo to create singleton.

  static TestTargetInfo* test_singleton_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_TEST_TEST_TARGET_INFO_H_
