# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_deb", "pkg_tar")
load("//bazel:rules.bzl", "HOST_ARCHES", "stratum_cc_binary", "stratum_license_tar")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

stratum_cc_binary(
    name = "stratum_dpdk",
    srcs = ["main.cc"],
    arches = HOST_ARCHES,
    data = [
        "dpdk_skip_p4.conf",
    ],
    defines = ["DPDK_TARGET"],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/phal",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/hal/lib/tdi:tdi_action_profile_manager",
        "//stratum/hal/lib/tdi:tdi_counter_manager",
        "//stratum/hal/lib/tdi:tdi_node",
        "//stratum/hal/lib/tdi:tdi_pre_manager",
        "//stratum/hal/lib/tdi:tdi_sde_wrapper",
        "//stratum/hal/lib/tdi:tdi_table_manager",
        "//stratum/hal/lib/tdi/dpdk:dpdk_chassis_manager",
        "//stratum/hal/lib/tdi/dpdk:dpdk_sde_target",
        "//stratum/hal/lib/tdi/dpdk:dpdk_sde_utils",
        "//stratum/hal/lib/tdi/dpdk:dpdk_switch",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

# Rules for Debian package
pkg_tar(
    name = "stratum_dpdk_binaries",
    srcs = [
        "start-stratum.sh",
        ":stratum_dpdk",
    ],
    mode = "0755",
    package_dir = "/usr/bin",
)

pkg_tar(
    name = "stratum_shareable_files",
    srcs = [
        "dpdk_skip_p4.conf",
    ],
    mode = "0644",
    package_dir = "/usr/share/stratum",
)

pkg_tar(
    name = "stratum_configs",
    srcs = [
        "//stratum/hal/lib/common:gnmi_caps.pb.txt",
    ],
    mode = "0644",
    package_dir = "/etc/stratum",
    deps = [
        "//stratum/hal/config:platform_configs_tar",
    ],
)

stratum_license_tar(
    name = "stratum_dpdk_legal",
    dep_notices = [
        "//legal:NOTICE.common.txt",
        "//legal:NOTICE.json.txt",
        "//legal:NOTICE.intel-sde.txt",
        # TODO(max): DPDK licenses
    ],
    extra_licenses = [
        "//legal:LICENSE.intel-sde.txt",
    ],
    mode = "0644",
    package_dir = "/usr/share/doc/stratum-dpdk/",
)

pkg_tar(
    name = "stratum_common_data",
    deps = [
        ":stratum_configs",
        ":stratum_shareable_files",
        "@local_dpdk_bin//:dpdk_library_files",
        "@local_dpdk_bin//:dpdk_shareable_files",
    ],
)

pkg_tar(
    name = "stratum_dpdk_data",
    extension = "tar.bz2",
    deps = [
        ":stratum_common_data",
        ":stratum_dpdk_binaries",
        ":stratum_dpdk_legal",
    ],
)

pkg_deb(
    name = "stratum_dpdk_deb",
    architecture = "amd64",
    data = ":stratum_dpdk_data",
    depends = [
        # TODO(max): check if really needed, copy & paste for now.
        "kmod",
        "libedit2",
        "libexpat1",
        "libssl1.1",
        "systemd",
        "telnet",
        # These are needed for sure.
        "libatomic1",
        "libpcap0.8",
        "libelf-dev",
    ],
    description = "The Stratum package for the P4 DPDK software platform",
    homepage = "https://stratumproject.org/",
    maintainer = "The Stratum Project",
    package = "stratum-dpdk",
    version = "0.0.1",
)
