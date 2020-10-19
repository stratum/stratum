# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

load(
    "//bazel:rules.bzl",
    "stratum_cc_library",
    "STRATUM_INTERNAL",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

# NOTE: All dependencies on @com_github_google_glog//:glog should be via the glue directory so that we
# can maintain flexible dependencies on portable google3.

# There are sections of code under status/ which is guarded by HERCULES. This
# target defines this.
stratum_cc_library(
    name = "define_hercules",
    defines = [
        "HERCULES",
    ],
)

stratum_cc_library(
    name = "init_google",
    hdrs = [
        "init_google.h",
    ],
    deps = [
        ":logging",
        "@com_github_gflags_gflags//:gflags",
    ],
)

stratum_cc_library(
    name = "logging",
    srcs = [
        "logging.cc",
    ],
    hdrs = [
        "logging.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
    ],
)

stratum_cc_library(
    name = "integral_types",
    hdrs = [
        "integral_types.h",
    ],
)
