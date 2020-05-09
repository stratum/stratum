# Copyright 2018-present Barefoot Networks, Inc.
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "HOST_ARCHES",
)
load(":bf_pi.bzl", "barefoot_pi_deps")

package(
    default_visibility = STRATUM_INTERNAL,
)

pi_node_common_deps = [
    "@com_google_googleapis//google/rpc:status_cc_proto",
    "@com_github_p4lang_p4runtime//:p4info_cc_proto",
    "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
    "//stratum/glue:integral_types",
    "//stratum/glue:logging",
    "//stratum/glue/status:status_macros",
    "//stratum/hal/lib/common:writer_interface",
    "//stratum/lib:constants",
    "//stratum/lib:macros",
    "//stratum/hal/lib/common:common_cc_proto",
]

# Default PI Node
stratum_cc_library(
    name = "pi_node",
    srcs = ["pi_node.cc"],
    hdrs = ["pi_node.h"],
    deps = pi_node_common_deps +
           ["@com_github_p4lang_PI//proto/frontend:pifeproto"]
)

barefoot_pi_deps(pi_node_common_deps)

# PI Node for Netcope targets
stratum_cc_library(
    name = "pi_node_np4",
    srcs = ["pi_node.cc"],
    hdrs = ["pi_node.h"],
    deps = pi_node_common_deps +
           [
           "@com_github_p4lang_PI_np4//proto/frontend:pifeproto",
           ]
)
