# Copyright 2021-present Open Networking Foundation
# Copyright 2020 Google LLC
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "p4runtime_session",
    srcs = ["p4runtime_session.cc"],
    hdrs = ["p4runtime_session.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:utils",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_role_config_cc_proto",
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
    name = "sdn_controller_manager",
    srcs = ["sdn_controller_manager.cc"],
    hdrs = ["sdn_controller_manager.h"],
    deps = [
        "//stratum/hal/lib/p4:utils",
        "//stratum/public/proto:p4_role_config_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

stratum_cc_library(
    name = "stream_message_reader_writer_mock",
    testonly = 1,
    hdrs = ["stream_message_reader_writer_mock.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_googletest//:gtest",
    ],
)
