# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_deb", "pkg_tar")
load("//bazel:rules.bzl", "HOST_ARCHES", "stratum_cc_binary", "stratum_license_tar")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

stratum_cc_binary(
    name = "bf_pipeline_builder",
    srcs = [
        "bf_pipeline_builder.cc",
    ],
    arches = HOST_ARCHES,
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/barefoot:bf_cc_proto",
        "//stratum/lib:utils",
        "@com_github_nlohmann_json//:json",
    ],
)

stratum_cc_binary(
    name = "stratum_bfrt",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    data = [
        "tofino_skip_p4.conf",
        "tofino_skip_p4_no_bsp.conf",
    ],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/barefoot:bf_chassis_manager",
        "//stratum/hal/lib/barefoot:bf_sde_wrapper",
        "//stratum/hal/lib/barefoot:bfrt_counter_manager",
        "//stratum/hal/lib/barefoot:bfrt_node",
        "//stratum/hal/lib/barefoot:bfrt_p4runtime_translator",
        "//stratum/hal/lib/barefoot:bfrt_pre_manager",
        "//stratum/hal/lib/barefoot:bfrt_switch",
        "//stratum/hal/lib/barefoot:bfrt_table_manager",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/phal",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "@com_github_google_glog//:glog",
    ],
)

# Rules for Debian package
pkg_tar(
    name = "stratum_bfrt_binaries",
    # TODO(bocon): Consider refactoring common start script and builder
    srcs = [
        "deb/attach-bf-shell.sh",
        "deb/start-stratum.sh",
        ":stratum_bfrt",
    ],
    mode = "0755",
    package_dir = "/usr/bin",
)

pkg_tar(
    name = "systemd_service",
    srcs = [
        "deb/stratum_bfrt.service",
    ],
    mode = "0644",
    package_dir = "/usr/lib/systemd/system",
)

pkg_tar(
    name = "stratum_shareable_files",
    srcs = [
        "tofino_skip_p4.conf",
        "tofino_skip_p4_no_bsp.conf",
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
    name = "stratum_bfrt_legal",
    dep_notices = [
        "//legal:NOTICE.common.txt",
        "//legal:NOTICE.json.txt",
        "//legal:NOTICE.intel-sde.txt",
    ],
    extra_licenses = [
        "//legal:LICENSE.intel-sde.txt",
    ],
    mode = "0644",
    package_dir = "/usr/share/doc/stratum-bfrt/",
)

pkg_tar(
    name = "stratum_common_data",
    deps = [
        ":stratum_configs",
        ":stratum_shareable_files",
        ":systemd_service",
        "@local_barefoot_bin//:bf_binary_files",
        "@local_barefoot_bin//:bf_library_files",
        "@local_barefoot_bin//:bf_shareable_files",
        "@local_barefoot_bin//:kernel_module",
    ],
)

pkg_tar(
    name = "stratum_bfrt_data",
    extension = "tar.bz2",
    deps = [
        ":stratum_bfrt_binaries",
        ":stratum_bfrt_legal",
        ":stratum_common_data",
    ],
)

# Extra SDE runtime dependencies when using the BSP.
# TODO(max): consider packaging these deps with the SDE itself somehow.
sde_bsp_debian_deps = [
    # Edgecore Wedge100bf BSP dependencies.
    "libusb-1.0-0",
    "libcurl4-gnutls-dev",
]

pkg_deb(
    name = "stratum_bfrt_deb",
    architecture = "amd64",
    data = ":stratum_bfrt_data",
    depends = [
        "kmod",
        "libatomic1",
        "libedit2",
        "libexpat1",
        "libssl1.1",
        "systemd",
        "telnet",
        "uml-utilities",  # for `tunctl` and CPU port workaround
    ] + sde_bsp_debian_deps,
    description = "The Stratum package for Barefoot Tofino-based platform",
    homepage = "https://stratumproject.org/",
    maintainer = "The Stratum Project",
    package = "stratum-bfrt",
    postinst = "deb/postinst",
    prerm = "deb/prerm",
    version = "0.0.1",
)
