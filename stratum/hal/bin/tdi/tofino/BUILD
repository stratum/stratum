# stratum/hal/bin/tdi/tofino/BUILD

# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_deb", "pkg_tar")
load("//bazel:rules.bzl", "HOST_ARCHES", "stratum_cc_binary")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

stratum_common_deps = [
    "//stratum/glue:init_google",
    "//stratum/glue:logging",
    "//stratum/hal/lib/phal:phal_sim",
    "//stratum/hal/lib/phal",
    "//stratum/lib/security:auth_policy_checker",
    "//stratum/lib/security:credentials_manager",
    "@com_github_google_glog//:glog",
]

stratum_cc_binary(
    name = "stratum_tofino",
    srcs = [
        "tofino_main.cc",
        "tofino_main.h",
        "main.cc",
    ],
    arches = HOST_ARCHES,
    defines = ["TOFINO_TARGET"],
    data = [
        "//stratum/hal/bin/barefoot:tofino_skip_p4.conf",
        "//stratum/hal/bin/barefoot:tofino_skip_p4_no_bsp.conf",
    ],
    deps = [
        "//stratum/hal/lib/tdi/tofino:tofino_chassis_manager",
        "//stratum/hal/lib/tdi/tofino:tofino_hal",
        "//stratum/hal/lib/tdi/tofino:tofino_sde_target",
        "//stratum/hal/lib/tdi/tofino:tofino_sde_utils",
        "//stratum/hal/lib/tdi/tofino:tofino_switch",
        "//stratum/hal/lib/tdi:tdi_action_profile_manager",
        "//stratum/hal/lib/tdi:tdi_counter_manager",
        "//stratum/hal/lib/tdi:tdi_node",
        "//stratum/hal/lib/tdi:tdi_pre_manager",
        "//stratum/hal/lib/tdi:tdi_sde_wrapper",
        "//stratum/hal/lib/tdi:tdi_table_manager",
    ] + stratum_common_deps,
)

# Rules for Debian package
pkg_tar(
    name = "stratum_tofino_binaries",
    srcs = [
        "deb/attach-bf-shell.sh",
        "deb/start-stratum.sh",
        "//stratum/hal/bin/tdi:tdi_pipeline_builder",
        ":stratum_tofino",
    ],
    mode = "0755",
    package_dir = "/usr/bin",
)

pkg_tar(
    name = "systemd_service",
    srcs = [
        "deb/stratum_tofino.service",
    ],
    mode = "0644",
    package_dir = "/usr/lib/systemd/system",
)

pkg_tar(
    name = "stratum_shareable_files",
    srcs = [
        "//stratum/hal/bin/barefoot:tofino_skip_p4.conf",
        "//stratum/hal/bin/barefoot:tofino_skip_p4_no_bsp.conf",
    ],
    mode = "0644",
    package_dir = "/usr/share/stratum",
)

pkg_tar(
    name = "stratum_configs",
    srcs = [
        "deb/stratum.flags",
        "deb/stratum_tofino.env",
        "//stratum/hal/lib/common:gnmi_caps.pb.txt",
    ],
    mode = "0644",
    package_dir = "/etc/stratum",
    deps = [
        "//stratum/hal/config:platform_configs_tar",
    ],
)

pkg_tar(
    name = "stratum_common_data",
    deps = [
        ":stratum_configs",
        ":stratum_shareable_files",
        ":systemd_service",
        "@local_tofino_bin//:tofino_library_files",
        "@local_tofino_bin//:tofino_shareable_files",
        "@local_tofino_bin//:tofino_kernel_module",
    ],
)

pkg_tar(
    name = "stratum_tofino_data",
    extension = "tar.bz2",
    deps = [
        ":stratum_tofino_binaries",
        ":stratum_common_data",
    ],
)

pkg_deb(
    name = "stratum_tofino_deb",
    architecture = "amd64",
    data = ":stratum_tofino_data",
    depends = [
        "kmod",
        "libedit2",
        "libssl1.1",
        "systemd",
        "telnet",
    ],
    description = "The Stratum package for an Intel Tofino-based platform",
    homepage = "https://stratumproject.org/",
    maintainer = "The Stratum Project",
    package = "stratum-tofino",
    postinst = "deb/postinst",
    prerm = "deb/prerm",
    version = "0.0.1",
)
