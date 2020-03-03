// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

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
