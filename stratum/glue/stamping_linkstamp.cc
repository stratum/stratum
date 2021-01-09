// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This file contains variables that will be populated by Bazel via the
// linkstamping feature. It must be keep as simple as possible to compile
// without headers, compiler flags or build variables.
// https://docs.bazel.build/versions/master/be/c-cpp.html#cc_library.linkstamp
// For a full list of supported variables, see:
// https://github.com/bazelbuild/bazel/blob/master/src/main/java/com/google/devtools/build/lib/bazel/BazelWorkspaceStatusModule.java#L316

extern const char kBuildHost[];
const char kBuildHost[] = BUILD_HOST;

extern const char kBuildScmRevision[];
const char kBuildScmRevision[] = BUILD_SCM_REVISION;

extern const char kBuildScmStatus[];
const char kBuildScmStatus[] = BUILD_SCM_STATUS;

extern const char kBuildUser[];
const char kBuildUser[] = BUILD_USER;

extern const int kBuildTimestamp;
const int kBuildTimestamp = BUILD_TIMESTAMP;
