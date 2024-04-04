# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

proto_library(
    name = "types_proto",
    srcs = ["gnoi/types/types.proto"],
    deps = [
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:descriptor_proto",
    ],
)

cc_proto_library(
    name = "types_cc_proto",
    deps = [":types_proto"],
)

proto_library(
    name = "common_proto",
    srcs = ["gnoi/common/common.proto"],
    deps = [":types_proto"],
)

cc_proto_library(
    name = "common_cc_proto",
    deps = [":common_proto"],
)

proto_library(
    name = "diag_proto",
    srcs = ["gnoi/diag/diag.proto"],
    deps = [":types_proto"],
)

cc_proto_library(
    name = "diag_cc_proto",
    deps = [":diag_proto"],
)

cc_grpc_library(
    name = "diag_cc_grpc",
    srcs = [":diag_proto"],
    grpc_only = True,
    deps = [":diag_cc_proto"],
)

proto_library(
    name = "system_proto",
    srcs = ["gnoi/system/system.proto"],
    deps = [
        ":common_proto",
        ":types_proto",
    ],
)

cc_proto_library(
    name = "system_cc_proto",
    deps = [":system_proto"],
)

cc_grpc_library(
    name = "system_cc_grpc",
    srcs = [":system_proto"],
    grpc_only = True,
    deps = [":system_cc_proto"],
)

proto_library(
    name = "file_proto",
    srcs = ["gnoi/file/file.proto"],
    deps = [
        ":common_proto",
        ":types_proto",
    ],
)

cc_proto_library(
    name = "file_cc_proto",
    deps = [":file_proto"],
)

cc_grpc_library(
    name = "file_cc_grpc",
    srcs = [":file_proto"],
    grpc_only = True,
    deps = [":file_cc_proto"],
)

proto_library(
    name = "cert_proto",
    srcs = ["gnoi/cert/cert.proto"],
    deps = [
        ":common_proto",
        ":types_proto",
    ],
)

cc_proto_library(
    name = "cert_cc_proto",
    deps = [":cert_proto"],
)

cc_grpc_library(
    name = "cert_cc_grpc",
    srcs = [":cert_proto"],
    grpc_only = True,
    deps = [":cert_cc_proto"],
)
