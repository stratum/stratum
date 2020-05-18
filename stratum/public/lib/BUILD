# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
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

stratum_cc_library(
    name = "error",
    srcs = [
        "error.cc",
    ],
    hdrs = [
        "error.h",
    ],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/public/proto:error_cc_proto",
    ],
)

cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "//stratum/glue:init_google",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "error_test",
    srcs = ["error_test.cc"],
    deps = [
        ":error",
        ":test_main",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)
