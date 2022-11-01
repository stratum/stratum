# bazel/external/dpdk.BUILD

# Copyright 2020-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

load("@//bazel/rules:package_rule.bzl", "pkg_tar_with_symlinks")
load("@rules_cc//cc:defs.bzl", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "dpdk_sde",
    srcs = glob([
        "dpdk-bin/lib/bfshell_plugin_*.so*",
        "dpdk-bin/lib/libbf_switchd_lib.so*",
        "dpdk-bin/lib/libclish.so*",
        "dpdk-bin/lib/libdpdk_infra.so*",
        "dpdk-bin/lib/libdriver.so*",
        "dpdk-bin/lib/libpython3*",
        "dpdk-bin/lib/libtarget_sys.so*",
        "dpdk-bin/lib/libtarget_utils.so*",
        "dpdk-bin/lib/libtdi_json_parser.so*",
        "dpdk-bin/lib/libtdi_pna.so*",
        "dpdk-bin/lib/libtdi.so*",
        "dpdk-bin/lib/x86_64-linux-gnu/librte_*.so*",  # DPDK libs
    ]),
    hdrs = glob([
        "dpdk-bin/include/bf_pal/*.h",
        "dpdk-bin/include/bf_rt/**/*.h",
        "dpdk-bin/include/bf_switchd/**/*.h",
        "dpdk-bin/include/bf_types/*.h",
        "dpdk-bin/include/cjson/*.h",
        "dpdk-bin/include/dvm/*.h",
        "dpdk-bin/include/lld/*.h",
        "dpdk-bin/include/osdep/*.h",
        "dpdk-bin/include/pipe_mgr/*.h",
        "dpdk-bin/include/port_mgr/**/*.h",
        "dpdk-bin/include/target-sys/**/*.h",
        "dpdk-bin/include/target-utils/**/*.h",
        "dpdk-bin/include/tdi/**/*.h",
        "dpdk-bin/include/tdi/**/*.hpp",
        "dpdk-bin/include/tdi_rt/**/*.h",
        "dpdk-bin/include/tdi_rt/**/*.hpp",
    ]),
    linkopts = [
        "-lpthread",
        "-lm",
        "-ldl",
    ],
    strip_include_prefix = "dpdk-bin/include",
)

# Runtime libraries
pkg_tar_with_symlinks(
    name = "dpdk_library_files",
    srcs = glob([
        "dpdk-bin/lib/bfshell_plugin_*.so*",
        "dpdk-bin/lib/libbf_switchd_lib.so*",
        "dpdk-bin/lib/libclish.so*",
        "dpdk-bin/lib/libdpdk_infra.so*",
        "dpdk-bin/lib/libdriver.so*",
        "dpdk-bin/lib/libpython3*",
        "dpdk-bin/lib/libtarget_sys.so*",
        "dpdk-bin/lib/libtarget_utils.so*",
        "dpdk-bin/lib/libtdi_json_parser.so*",
        "dpdk-bin/lib/libtdi_pna.so*",
        "dpdk-bin/lib/libtdi.so*",
        "dpdk-bin/lib/x86_64-linux-gnu/librte_*.so*",  # DPDK libs
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "dpdk-bin",
)

pkg_tar_with_symlinks(
    name = "dpdk_shareable_files",
    srcs = glob([
        "dpdk-bin/share/bf_rt_shared/**",
        "dpdk-bin/share/cli/xml/**",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "dpdk-bin",
)
