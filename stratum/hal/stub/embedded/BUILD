# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_package",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_binary(
    name = "stratum_stub",
    srcs = ["main.cc"],
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lrt",
    ],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:openconfig_converter",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_package(
    name = "stratum_stub_pkg",
    bins = [
        ":stratum_stub",
    ],
)
