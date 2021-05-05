# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "status",
    srcs = [
        "canonical_errors.cc",
        "posix_error_space.cc",
        "status.cc",
    ],
    hdrs = [
        "canonical_errors.h",
        "posix_error_space.h",
        "status.h",
    ],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:source_location",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "statusor",
    srcs = [
        "statusor.cc",
    ],
    hdrs = [
        "statusor.h",
    ],
    deps = [
        ":status",
        "//stratum/glue:logging",
    ],
)

stratum_cc_library(
    name = "status_macros",
    srcs = [
        "status_macros.cc",
    ],
    hdrs = [
        "status_macros.h",
    ],
    deps = [
        ":status",
        ":statusor",
        "//stratum/glue:logging",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_library(
    name = "status_test_util",
    testonly = 1,
    srcs = [
        "status_test_util.cc",
    ],
    hdrs = [
        "status_test_util.h",
    ],
    deps = [
        ":status",
        ":statusor",
        "//stratum/public/proto:error_cc_proto",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "status_test",
    size = "small",
    srcs = [
        "posix_error_space_test.cc",
        "status_test.cc",
        "status_test_util_test.cc",
        "statusor_test.cc",
    ],
    deps = [
        ":status",
        ":status_test_util",
        ":statusor",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
