# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")
load("//bazel:defs.bzl", "STRATUM_INTERNAL")

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

proto_library(
    name = "error_proto",
    srcs = ["error.proto"],
)

cc_proto_library(
    name = "error_cc_proto",
    deps = [":error_proto"],
)

proto_library(
    name = "p4_annotation_proto",
    srcs = ["p4_annotation.proto"],
    deps = [":p4_table_defs_proto"],
)

cc_proto_library(
    name = "p4_annotation_cc_proto",
    deps = [":p4_annotation_proto"],
)

proto_library(
    name = "p4_table_defs_proto",
    srcs = ["p4_table_defs.proto"],
)

cc_proto_library(
    name = "p4_table_defs_cc_proto",
    deps = [":p4_table_defs_proto"],
)

proto_library(
    name = "openconfig_goog_bcm_proto",
    srcs = ["openconfig-goog-bcm.proto"],
    deps = ["@com_github_openconfig_ygot_proto//:ywrapper_proto"],
)

cc_proto_library(
    name = "openconfig_goog_bcm_cc_proto",
    deps = [":openconfig_goog_bcm_proto"],
)

''' FIXME(boc) expose gNMI and P4RT protos
proto_library(
    name = "tv_proto",
    srcs = ["tv.proto"],
    #cc_api_version = 2, FIXME(boc) google only
    deps = [
        "@com_github_openconfig_gnmi//:proto/gnmi/gnmi.proto",
        "@com_github_p4lang_p4runtime//:proto/p4/v1/p4runtime.proto",
    ],
)
'''

filegroup(
    name = "proto_srcs",
    srcs = glob(["*.proto"]),
)
