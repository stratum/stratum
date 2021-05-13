# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("//bazel:rules.bzl", "HOST_ARCHES", "stratum_cc_binary")
load("@rules_pkg//:pkg.bzl", "pkg_deb", "pkg_tar")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

stratum_cc_binary(
    name = "stratum_bmv2",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    data = ["dummy.json"],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/bmv2:bmv2_switch",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/hal/lib/pi:pi_node",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "@com_github_google_glog//:glog",
        "@local_bmv2_bin//:bmv2_headers",
        "@local_bmv2_bin//:bmv2_pi",
        "@local_bmv2_bin//:bmv2_simple_switch",
    ],
)

pkg_tar(
    name = "stratum_bmv2_bin",
    srcs = [":stratum_bmv2"],
    mode = "0755",
    package_dir = "/usr/bin",
    strip_prefix = "/stratum/hal/bin/bmv2",
)

pkg_tar(
    name = "stratum_bmv2_service",
    srcs = ["stratum_bmv2.service"],
    mode = "0644",
    package_dir = "/etc/systemd/system",
    strip_prefix = "/stratum/hal/bin/bmv2",
)

pkg_tar(
    name = "stratum_bmv2_config",
    srcs = [
        "chassis_config.pb.txt",
        "stratum_bmv2.environment",
    ],
    mode = "0644",
    package_dir = "/etc/stratum",
    strip_prefix = "/stratum/hal/bin/bmv2",
)

pkg_tar(
    name = "stratum_bmv2_pipeline",
    srcs = ["dummy.json"],
    mode = "0644",
    package_dir = "/etc/stratum",
    strip_prefix = "/stratum/hal/bin/bmv2",
)

pkg_tar(
    name = "stratum_bmv2_data",
    extension = "tar.gz",
    deps = [
        ":stratum_bmv2_bin",
        ":stratum_bmv2_config",
        ":stratum_bmv2_pipeline",
        ":stratum_bmv2_service",
        "@local_bmv2_bin//:bmv2_library_files",
    ],
)

pkg_deb(
    name = "stratum_bmv2_deb",
    architecture = "amd64",
    data = ":stratum_bmv2_data",
    depends = [
        "libboost-filesystem1.62.0",
        "libboost-program-options1.62.0",
        "libboost-thread1.62.0",
        "libgmp10",
        "libjudydebian1",
        "libpcap-dev",
    ],
    description = "The Stratum package for BMv2 software switches",
    homepage = "https://stratumproject.org/",
    maintainer = "The Stratum Project",
    package = "stratum-bmv2",
    postinst = "debian/postinst",
    version = "0.0.1",
)
