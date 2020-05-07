# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

load("//bazel:rules.bzl", "stratum_cc_binary", "HOST_ARCHES")

stratum_cc_binary(
    name = "stratum_bf",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    deps = [
        "@com_github_google_glog//:glog",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/barefoot:bf_chassis_manager",
        "//stratum/hal/lib/barefoot:bf_pal_wrapper",
        "//stratum/hal/lib/barefoot:bf_switch",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/hal/lib/phal:phal",
        "//stratum/hal/lib/pi:pi_node_bf",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "@local_barefoot_bin//:bf_headers",
        "@local_barefoot_bin//:bf_switchd",
        "@local_barefoot_bin//:bf_drivers",
        "@local_barefoot_bin//:bf_sys",
        "@local_barefoot_bin//:bf_utils",
    ],
    data = [
        "tofino_skip_p4.conf",
        "tofino_skip_p4_no_bsp.conf",
        "platforms/x86-64-accton-wedge100bf-32x-r0.json",
        "platforms/x86-64-delta-ag9064v1-r0.json",
        "platforms/x86-64-inventec-d5254-r0.json"
    ],
)
