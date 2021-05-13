# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "entity_management",
    srcs = ["entity_management.cc"],
    hdrs = ["entity_management.h"],
    deps = [
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_library(
    name = "p4runtime_session",
    srcs = ["p4runtime_session.cc"],
    hdrs = ["p4runtime_session.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:utils",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_public_hdrs",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

stratum_cc_library(
    name = "p4runtime_fixture",
    testonly = 1,
    # srcs = ["p4runtime_fixture.cc"],
    hdrs = ["p4runtime_fixture.h"],
    deps = [
        ":p4runtime_session",
        "//stratum/lib:utils",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ],
)

