# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("//bazel:rules.bzl", "stratum_cc_binary", "HOST_ARCHES")
licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

stratum_cc_binary(
    name = "dummy_cli",
    srcs = [
        "dummy_cli.cc"
    ],
    deps = [
        "@com_github_gflags_gflags//:gflags",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_protobuf//:protobuf",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_proto",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "//stratum/hal/lib/dummy:dummy_test_cc_proto",
        "//stratum/hal/lib/dummy:dummy_test_cc_grpc",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/glue:integral_types"
    ]
)

stratum_cc_binary(
    name = "port_counter_sim",
    srcs = [
        "port_counter_sim.cc"
    ],
    deps = [
        "@com_github_gflags_gflags//:gflags",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/time:time",
        "@com_google_protobuf//:protobuf",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_proto",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "//stratum/hal/lib/dummy:dummy_test_cc_proto",
        "//stratum/hal/lib/dummy:dummy_test_cc_grpc",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/glue:integral_types"
    ]
)

stratum_cc_binary(
    name = "stratum_dummy",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    deps = [
        "@com_github_google_glog//:glog",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/phal:phal",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "//stratum/hal/lib/dummy:dummy_switch",
        "//stratum/hal/lib/dummy:dummy_box",
    ],
)
