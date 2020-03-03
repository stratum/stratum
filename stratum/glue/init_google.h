// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_GLUE_INIT_GOOGLE_H_
#define STRATUM_GLUE_INIT_GOOGLE_H_

// TODO(boc): google only: this is not required for open source
// #include "base/init_google.h"  // IWYU pragma: export

// InitGoogle is not defined in portable base, so we instead emulate its
// behavior.
#if !GOOGLE_BASE_HAS_INITGOOGLE

#include "gflags/gflags.h"

// TODO(unknown) need to define transformation or comment this out on
// Google's side
using gflags::ParseCommandLineFlags;

inline void InitGoogle(const char *usage, int *argc, char ***argv,
                       bool remove_flags) {
  ParseCommandLineFlags(argc, argv, remove_flags);
}

#endif  // !GOOGLE_BASE_HAS_INITGOOGLE

#endif  // STRATUM_GLUE_INIT_GOOGLE_H_
