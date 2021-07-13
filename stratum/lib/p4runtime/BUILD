# Copyright 2020 Google LLC
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "stratum_cc_library",
)

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],
)

stratum_cc_library(
    name = "sdn_controller_manager",
    srcs = ["sdn_controller_manager.cc"],
    hdrs = ["sdn_controller_manager.h"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
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
