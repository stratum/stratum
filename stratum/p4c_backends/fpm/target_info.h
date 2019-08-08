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

// TargetInfo is an interface class that provides details about specific
// p4c backend target platforms.  Targets can correspond to vendors, e.g.
// "BCM", they can be a "mock" or "test" target for unit tests, or they can
// potentially be specific to certain chips or chip versions for the same
// vendor.

#ifndef STRATUM_P4C_BACKENDS_FPM_TARGET_INFO_H_
#define STRATUM_P4C_BACKENDS_FPM_TARGET_INFO_H_

#include "stratum/public/proto/p4_annotation.pb.h"

namespace stratum {
namespace p4c_backends {

class TargetInfo {
 public:
  virtual ~TargetInfo() {}

  // InjectSingleton sets up the singleton TargetInfo instance when the
  // p4c backend initializes or during unit test case setup.  Unit tests
  // may call InjectSingleton with nullptr when finished with a particular
  // singleton.
  static void InjectSingleton(TargetInfo* target_info);

  // GetSingleton returns the singleton TargetInfo instance.  InjectSingleton
  // must be called first to provide the instance, or GetSingleton fails
  // fatally.
  static TargetInfo* GetSingleton();

  // IsPipelineStageFixed evaluates the input pipeline stage and returns true
  // if it matches a fixed-function stage of the target's forwarding pipeline
  // hardware.
  virtual bool IsPipelineStageFixed(
      P4Annotation::PipelineStage stage) const = 0;

 private:
  static TargetInfo* singleton_;  // Singleton instance of this class.
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_TARGET_INFO_H_
