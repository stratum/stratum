# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_package",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
    # TODO(b/76020938); doesn't work with header modules yet.
    features = ["-use_header_modules"],
)

stratum_cc_binary(
    name = "stratum_sim",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lrt",
        "-lutil",
    ],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/bcm:bcm_acl_manager",
        "//stratum/hal/lib/bcm:bcm_chassis_manager",
        "//stratum/hal/lib/bcm:bcm_l2_manager",
        "//stratum/hal/lib/bcm:bcm_l3_manager",
        "//stratum/hal/lib/bcm:bcm_node",
        "//stratum/hal/lib/bcm:bcm_packetio_manager",
        "//stratum/hal/lib/bcm:bcm_sdk_diag_shell",
        "//stratum/hal/lib/bcm:bcm_sdk_sim",
        "//stratum/hal/lib/bcm:bcm_serdes_db_manager",
        "//stratum/hal/lib/bcm:bcm_switch",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_package(
    name = "stratum_pkg",
    bins = [
        ":stratum_sim",
    ],
)
