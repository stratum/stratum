# Copyright 2019-present Barefoot Networks, Inc.
# Copyright 2019-present Dell EMC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

load("//bazel:rules.bzl", "HOST_ARCHES", "stratum_cc_binary")

stratum_cc_binary(
    name = "stratum_np4intel",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    data = ["dummy.json"],
    deps = [
        "@com_github_google_glog//:glog",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/lib:utils",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/hal/lib/phal",
        "//stratum/hal/lib/pi:pi_node_np4",
        "//stratum/hal/lib/np4intel:np4_switch",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "@local_np4intel_bin//:np4_atom",
        #"@com_github_p4lang_PI_np4//proto/frontend:pifeproto",
        "@com_github_p4lang_PI_np4//targets/np4:pi_np4",
        ":dpdk_cc_proto",
    ],
)

load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_deb", "pkg_tar")

pkg_tar(
    name = "stratum_np4intel_bin",
    srcs = [":stratum_np4intel"],
    mode = "0755",
    package_dir = "/usr/bin",
    strip_prefix = "/stratum/hal/bin/np4intel",
)

pkg_tar(
    name = "stratum_np4intel_service",
    srcs = ["stratum_np4intel.service"],
    mode = "0644",
    package_dir = "/etc/systemd/system",
    strip_prefix = "/stratum/hal/bin/np4intel",
)

pkg_tar(
    name = "stratum_np4intel_config",
    srcs = ["stratum_np4intel.environment"],
    mode = "0644",
    package_dir = "/etc/stratum",
    strip_prefix = "/stratum/hal/bin/np4intel",
)

pkg_tar(
    name = "stratum_np4intel_pipeline",
    srcs = ["dummy.json"],
    mode = "0644",
    package_dir = "/etc/stratum",
    strip_prefix = "/stratum/hal/bin/np4intel",
)

pkg_tar(
    name = "stratum_np4intel_data",
    extension = "tar.gz",
    deps = [
        ":stratum_np4intel_bin",
        ":stratum_np4intel_config",
        ":stratum_np4intel_pipeline",
        ":stratum_np4intel_service",
    ],
)

pkg_deb(
    name = "stratum_np4intel_debian",
    architecture = "amd64",
    built_using = "bazel (0.14.1)",
    data = ":stratum_np4intel_data",
    depends = [
        "libjudy-dev",
        "libgmp-dev",
        "libpcap-dev",
        "libboost1.55-all-dev",
    ],
    description_file = "debian/description",
    homepage = "https://stratumproject.org/",
    maintainer = "https://stratumproject.org/",
    package = "stratum_np4intel",
    postinst = "debian/postinst",
    version = "0.1.1",
)

proto_library(
    name = "dpdk_proto",
    srcs = ["dpdk_config.proto"],
    deps = [
    ],
)

cc_proto_library(
    name = "dpdk_cc_proto",
    deps = [":dpdk_proto"],
)
