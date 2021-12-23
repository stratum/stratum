# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "@com_github_stratum_stratum//bazel/rules:proto_rule.bzl",
    "wrapped_proto_library",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

PREFIX = "github.com/openconfig/ygot/proto/"

wrapped_proto_library(
    name = "ywrapper_proto",
    srcs = ["proto/ywrapper/ywrapper.proto"],
    new_proto_dir = PREFIX,
    proto_source_root = "proto/",
)

wrapped_proto_library(
    name = "yext_proto",
    srcs = ["proto/yext/yext.proto"],
    new_proto_dir = PREFIX,
    proto_source_root = "proto/",
    deps = ["@com_google_protobuf//:descriptor_proto"],
)

cc_proto_library(
    name = "ywrapper_cc_proto",
    deps = [":ywrapper_proto"],
)

cc_proto_library(
    name = "yext_cc_proto",
    deps = [":yext_proto"],
)
