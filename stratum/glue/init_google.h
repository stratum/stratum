/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

inline void InitGoogle(const char* usage, int* argc, char*** argv,
                       bool remove_flags) {
  ParseCommandLineFlags(argc, argv, remove_flags);
}

#endif  // !GOOGLE_BASE_HAS_INITGOOGLE

#endif  // STRATUM_GLUE_INIT_GOOGLE_H_
