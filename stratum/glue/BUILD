# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

# NOTE: All dependencies on @com_github_google_glog//:glog should be via the
# glue directory so that we can maintain flexible dependencies on portable
# google3.

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
        ":stamping",
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
        ":stamping",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/time",
    ],
)

stratum_cc_library(
    name = "integral_types",
    hdrs = [
        "integral_types.h",
    ],
)

cc_library(
    name = "stamping",
    hdrs = ["stamping.h"],
    linkstamp = "stamping_linkstamp.cc",
)
