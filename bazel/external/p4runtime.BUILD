# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_grpc_library", "py_proto_library")

package(
    default_visibility = ["//visibility:public"],
)

proto_library(
    name = "p4types_proto",
    srcs = [
        "p4/config/v1/p4types.proto",
    ],
)

cc_proto_library(
    name = "p4types_cc_proto",
    deps = [":p4types_proto"],
)

proto_library(
    name = "p4info_proto",
    srcs = [
        "p4/config/v1/p4info.proto",
    ],
    deps = [
        ":p4types_proto",
        "@com_google_protobuf//:descriptor_proto",
        "@com_google_protobuf//:any_proto",
    ],
)

cc_proto_library(
    name = "p4info_cc_proto",
    deps = [":p4info_proto"],
)

proto_library(
    name = "p4data_proto",
    srcs = ["p4/v1/p4data.proto"],
)

cc_proto_library(
    name = "p4data_cc_proto",
    deps = [":p4data_proto"],
)

proto_library(
    name = "p4runtime_proto",
    srcs = ["p4/v1/p4runtime.proto"],
    deps = [
        ":p4info_proto",
        ":p4data_proto",
        "@com_google_protobuf//:any_proto",
        "@com_google_googleapis//google/rpc:status_proto"
    ],
)

cc_proto_library(
    name = "p4runtime_cc_proto",
    deps = [
        ":p4runtime_proto",
    ],
)

cc_grpc_library(
    name = "p4runtime_cc_grpc",
    srcs = [
        ":p4runtime_proto"
    ],
    deps = [
        ":p4runtime_cc_proto"
    ],
    grpc_only = True
)

py_proto_library(
    name = "p4runtime_py_proto",
    deps = [":p4runtime_proto"],
)

py_grpc_library(
    name = "p4runtime_py_grpc",
    srcs = [":p4runtime_proto"],
    deps = [":p4runtime_py_proto"],
)
