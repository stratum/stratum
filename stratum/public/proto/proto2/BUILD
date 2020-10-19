# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# Google3 net/proto2 versions of stratum/public/proto
# protobufs.

# Including both third-party proto3 libraries & google3 proto libraries
# leads to compile-time name collisions. These build rules generate google3
# proto2 libraries that will compile alongside other google3 proto libraries.

load("@rules_proto//proto:defs.bzl", "proto_library")
load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

genrule(
    name = "proto2_srcs",
    srcs = ["//stratum/public/proto:proto_srcs"],
    outs = [
        "p4_table_defs.proto",
        "p4_annotation.proto",
    ],
    # Replace:
    #   import "stratum/.../pkg_proto"
    # with...
    #   import "stratum/.../proto2/pkg_proto"
    cmd = ("cp $(SRCS) $(@D);" +
           "sed -i 's|\\(import.*stratum/.*\\)/|\\1/proto2/|' $(OUTS);"),
    visibility = ["//visibility:private"],
)

proto_library(
    name = "p4_table_defs_proto",
    srcs = ["p4_table_defs.proto"],
    #cc_api_version = 2, FIXME(boc) google only
    data = [":proto2_srcs"],
)

proto_library(
    name = "p4_annotation_proto",
    srcs = ["p4_annotation.proto"],
    data = [":proto2_srcs"],
    #cc_api_version = 2, FIXME(boc) google only
    deps = [":p4_table_defs_proto"],
)
