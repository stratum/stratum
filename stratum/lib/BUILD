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
    name = "constants",
    hdrs = ["constants.h"],
    deps = [
        "//stratum/glue:integral_types",
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_library(
    name = "macros",
    hdrs = ["macros.h"],
    deps = [
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_test(
    name = "macros_test",
    srcs = ["macros_test.cc"],
    deps = [
        ":macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "timer_daemon",
    srcs = ["timer_daemon.cc"],
    hdrs = ["timer_daemon.h"],
    deps = [
        ":macros",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/public/lib:error",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

stratum_cc_test(
    name = "timer_daemon_test",
    srcs = ["timer_daemon_test.cc"],
    deps = [
        ":test_main",
        ":timer_daemon",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":macros",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/public/lib:error",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    copts = ["-funsigned-char"],
    deps = [
        ":test_main",
        ":utils",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_googletest//:gtest",
    ],
)
