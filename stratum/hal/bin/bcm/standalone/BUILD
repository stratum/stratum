# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# Copyright 2019 Broadcom. All rights reserved. The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_tar", "pkg_deb")

licenses(["notice"])  # Apache v2

exports_files(["LICENSE"])

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_package",
    "EMBEDDED_ARCHES",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

licenses(["notice"])  # Apache v2

stratum_bcm_common_deps = [
    "@com_github_google_glog//:glog",
    "@com_google_absl//absl/memory",
    "@com_google_absl//absl/synchronization",
    "//stratum/glue:init_google",
    "//stratum/glue:logging",
    "//stratum/hal/lib/bcm:bcm_acl_manager",
    "//stratum/hal/lib/bcm:bcm_chassis_manager",
    "//stratum/hal/lib/bcm:bcm_l2_manager",
    "//stratum/hal/lib/bcm:bcm_l3_manager",
    "//stratum/hal/lib/bcm:bcm_node",
    "//stratum/hal/lib/bcm:bcm_packetio_manager",
    "//stratum/hal/lib/bcm:bcm_sdk_diag_shell",
    "//stratum/hal/lib/bcm:bcm_sdk_wrapper",
    "//stratum/hal/lib/bcm:bcm_serdes_db_manager",
    "//stratum/hal/lib/bcm:bcm_switch",
    "//stratum/hal/lib/bcm:bcm_table_manager",
    "//stratum/hal/lib/bcm:bcm_tunnel_manager",
    "//stratum/hal/lib/common:hal",
    "//stratum/hal/lib/p4:p4_table_mapper",
    "//stratum/hal/lib/phal:phal",
    "//stratum/lib/security:auth_policy_checker",
    "//stratum/lib/security:credentials_manager",
]

stratum_cc_binary(
    name = "stratum_bcm_sdklt",
    srcs = [
        "main.cc",
    ],
    arches = EMBEDDED_ARCHES,
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lutil",
        "-lrt",
        "-lyaml",
    ],
    deps = stratum_bcm_common_deps,
)

stratum_cc_binary(
    name = "stratum_bcm_opennsa",
    srcs = [
        "main.cc",
    ],
    arches = EMBEDDED_ARCHES,
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lutil",
    ],
    deps = stratum_bcm_common_deps,
)

stratum_package(
    name = "stratum_pkg",
    bins = [
        ":stratum_hal",
    ],
)

pkg_tar(
    name = "stratum_bcm_sdklt_bin",
    srcs = [
        ":stratum_bcm_sdklt",
        "sdklt/start-stratum.sh",
    ],
    mode = "0755",
    package_dir = "/usr/bin",
)

pkg_tar(
    name = "stratum_bcm_opennsa_bin",
    srcs = [
        ":stratum_bcm_opennsa",
        "sdk/start-stratum.sh",
    ],
    mode = "0755",
    package_dir = "/usr/bin",
)

pkg_tar(
    name = "stratum_bcm_sdklt_lib",
    srcs = [
        "@com_github_opennetworkinglab_sdklt//:kernel_modules",
    ],
    package_dir = "/usr/lib/stratum",
)

pkg_tar(
    name = "stratum_bcm_opennsa_lib",
    srcs = [
        "@com_github_broadcom_opennsa//:kernel_modules",
    ],
    package_dir = "/usr/lib/stratum",
)

pkg_tar(
    name = "stratum_bcm_etc",
    deps = [
        "//stratum/hal/config:all_configs_tar",
    ],
    package_dir = "/etc/stratum",
)

pkg_tar(
    name = "statum_bcm_sdklt_all",
    deps = [
        ":stratum_bcm_sdklt_bin",
        ":stratum_bcm_sdklt_lib",
        ":stratum_bcm_etc",
    ],
)

pkg_tar(
    name = "statum_bcm_opennsa_all",
    deps = [
        ":stratum_bcm_opennsa_bin",
        ":stratum_bcm_opennsa_lib",
        ":stratum_bcm_etc",
    ],
)

pkg_deb(
    name = "stratum_bcm_opennsa_deb",
    package = "stratum-bcm-opennsa",
    version = "0.0.1",
    description = "Stratum for Broadcom switches using OpenNSA.",
    architecture = "amd64",
    data = ":statum_bcm_opennsa_all",
    maintainer = "https://stratumproject.org/",
    conflicts = ["stratum-bcm-sdklt"],
)

pkg_deb(
    name = "stratum_bcm_sdklt_deb",
    package = "stratum-bcm-sdklt",
    version = "0.0.1",
    description = "Stratum for Broadcom switches using SDKLT.",
    architecture = "amd64",
    data = ":statum_bcm_sdklt_all",
    maintainer = "https://stratumproject.org/",
    conflicts = ["stratum-bcm-opennsa"],
)
