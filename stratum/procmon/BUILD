# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_package",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

proto_library(
    name = "procmon_proto",
    srcs = ["procmon.proto"],
)

cc_proto_library(
    name = "procmon_cc_proto",
    deps = [":procmon_proto"],
)

cc_grpc_library(
    name = "procmon_cc_grpc",
    srcs = [":procmon_proto"],
    grpc_only = True,
    deps = [":procmon_cc_proto"],
)

stratum_cc_library(
    name = "procmon",
    srcs = ["procmon.cc"],
    hdrs = ["procmon.h"],
    deps = [
        ":procmon_cc_grpc",
        ":procmon_cc_proto",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "procmon_service_impl",
    srcs = ["procmon_service_impl.cc"],
    hdrs = ["procmon_service_impl.h"],
    deps = [
        ":procmon_cc_grpc",
        ":procmon_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

stratum_cc_test(
    name = "procmon_test",
    srcs = ["procmon_test.cc"],
    deps = [
        ":procmon",
        ":procmon_cc_grpc",
        ":procmon_cc_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_binary(
    name = "procmon_main",
    srcs = ["procmon_main.cc"],
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lrt",
    ],
    deps = [
        ":procmon",
        ":procmon_cc_grpc",
        ":procmon_cc_proto",
        ":procmon_service_impl",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        #"//sandblaze/prebuilt/openssl",
    ],
)

stratum_package(
    name = "procmon_main_pkg",
    bins = [
        ":procmon_main",
    ],
)
