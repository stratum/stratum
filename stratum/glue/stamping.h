// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_GLUE_STAMPING_H_
#define STRATUM_GLUE_STAMPING_H_

namespace stratum {
// Populated via linkstamping.
extern const char kBuildHost[];
extern const char kBuildScmRevision[];
extern const char kBuildScmStatus[];
extern const char kBuildUser[];
extern const int kBuildTimestamp;
}  // namespace stratum

#endif  // STRATUM_GLUE_STAMPING_H_
