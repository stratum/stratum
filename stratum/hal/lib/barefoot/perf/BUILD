# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl",
     "cc_grpc_library")
load("@com_github_grpc_grpc//bazel:python_rules.bzl",
     "py_proto_library", "py_grpc_library")

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_cc_binary",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

proto_library(
    name = "any_proto",
    srcs = ["any.proto"],
    #import_prefix = "bfrt_proto",
    #strip_import_prefix = "/stratum/hal/lib/barefoot/perf",
)

proto_library(
    name = "status_proto",
    srcs = ["status.proto"],
    deps = [
        ":any_proto"
    ],
    #import_prefix = "bfrt_proto",
    #strip_import_prefix = "/stratum/hal/lib/barefoot/perf",
)

proto_library(
    name = "bfruntime_proto",
    srcs = ["bfruntime.proto"],
    deps = [
        #"@com_google_googleapis//google/rpc:status_proto"
        ":status_proto"
    ],
    #import_prefix = "bfrt_proto",
    #strip_import_prefix = "/stratum/hal/lib/barefoot/perf",
)

cc_proto_library(
    name = "bfruntime_cc_proto",
    deps = [":bfruntime_proto"],
)

cc_grpc_library(
    name = "bfruntime_cc_grpc",
    srcs = [":bfruntime_proto"],
    deps = [":bfruntime_cc_proto"],
    grpc_only = True
)

py_proto_library(
    name = "any_py_pb2",
    #deps = ["@com_google_googleapis//google/rpc:status_proto"],
    deps = [":any_proto"],
)

py_proto_library(
    name = "status_py_pb2",
    #deps = ["@com_google_googleapis//google/rpc:status_proto"],
    deps = [":status_proto"],
)

py_proto_library(
    name = "bfruntime_py_pb2",
    deps = [":bfruntime_proto"],
)

py_grpc_library(
    name = "bfruntime_py_pb2_grpc",
    srcs = [":bfruntime_proto"],
    deps = [":bfruntime_py_pb2"],
)

stratum_cc_library(
    name = "bfruntime",
    hdrs = ["bfruntime.h"],
    srcs = ["bfruntime.cc"],
    deps = [
        ":bfruntime_cc_grpc",
        ":bfruntime_cc_proto",
        #"@local_barefoot_bin//:bfruntime_cc_grpc",
        #"@local_barefoot_bin//:bfruntime_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/time",
    ],
)

stratum_cc_binary(
    name = "server",
    srcs = ["server.cc"],
    deps = [
        ":bfruntime",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

stratum_cc_binary(
    name = "client",
    srcs = ["client.cc"],
    deps = [
        ":bfruntime_cc_grpc",
        ":bfruntime_cc_proto",
        #"@local_barefoot_bin//:bfruntime_cc_grpc",
        #"@local_barefoot_bin//:bfruntime_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/time",
    ],
)

py_binary(
    name = "client_py",
    main = "client.py",
    srcs = ["client.py"],
    deps = [
        ":bfruntime_py_pb2",
        ":bfruntime_py_pb2_grpc",
        ":status_py_pb2",
        ":any_py_pb2",
	"@com_github_grpc_grpc//src/python/grpcio/grpc:grpcio",
	"@com_google_protobuf//:protobuf_python",
    ],
    python_version="PY2",
)
