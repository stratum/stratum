// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_GLUE_INIT_GOOGLE_H_
#define STRATUM_GLUE_INIT_GOOGLE_H_

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/stamping.h"

inline void InitGoogle(const char* usage, int* argc, char*** argv,
                       bool remove_flags) {
  // Here we can set Stratum-wide defaults or overrides for library flags.
  // No logging to files, stderr only.
  CHECK(!::gflags::SetCommandLineOptionWithMode("logtostderr", "true",
                                                ::gflags::SET_FLAGS_DEFAULT)
             .empty());
  CHECK(!::gflags::SetCommandLineOptionWithMode("colorlogtostderr", "true",
                                                ::gflags::SET_FLAGS_DEFAULT)
             .empty());
  // Note: stderrthreshold is only meaningful if logtostderr == false!
  CHECK(!::gflags::SetCommandLineOptionWithMode("stderrthreshold", "0",
                                                ::gflags::SET_FLAGS_DEFAULT)
             .empty());
  CHECK(!::gflags::SetCommandLineOptionWithMode("minloglevel", "0",
                                                ::gflags::SET_FLAGS_DEFAULT)
             .empty());
  ::gflags::SetVersionString(stratum::kBuildScmRevision);
  ::gflags::ParseCommandLineFlags(argc, argv, remove_flags);
}

#endif  // STRATUM_GLUE_INIT_GOOGLE_H_
