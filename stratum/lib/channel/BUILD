# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

# This package contains message-passing libraries for Stratum.

load(
    "//bazel:rules.bzl",
    "stratum_cc_library",
    "stratum_cc_test",
    "STRATUM_INTERNAL",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "channel",
    srcs = [
        "channel.cc",
    ],
    hdrs = [
        "channel.h",
        "channel_internal.h",
    ],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "channel_mock",
    testonly = 1,
    hdrs = [
        "channel_mock.h",
    ],
    deps = [
        ":channel",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)

stratum_cc_test(
    name = "channel_test",
    srcs = [
        "channel_test.cc",
    ],
    deps = [
        ":channel",
        ":test_main",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
    ],
)
