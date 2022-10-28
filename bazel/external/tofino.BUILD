# Copyright 2020-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

load("@//bazel/rules:package_rule.bzl", "pkg_tar_with_symlinks")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("@bazel_skylib//rules:common_settings.bzl", "string_setting")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "tofino_sde",
    srcs = glob([
        "tofino-bin/lib/libdriver.so",
        "tofino-bin/lib/libpython3.10m.so*",
        "tofino-bin/lib/libtarget_sys.so",
    ]),
    hdrs = glob([
        "tofino-bin/include/bf_switchd/*.h",
        "tofino-bin/include/bf_types/*.h",
        "tofino-bin/include/bfutils/**/*.h",
        "tofino-bin/include/dvm/*.h",
        "tofino-bin/include/lld/*.h",
        "tofino-bin/include/mc_mgr/*.h",
        "tofino-bin/include/pipe_mgr/*.h",
        "tofino-bin/include/pkt_mgr/*.h",
        "tofino-bin/include/port_mgr/*.h",
        "tofino-bin/include/target-sys/**/*.h",
        "tofino-bin/include/target-utils/**/*.h",
        "tofino-bin/include/tdi/**/*.h",
        "tofino-bin/include/tdi/**/*.hpp",
        "tofino-bin/include/tdi/*.h",
        "tofino-bin/include/tdi/*.hpp",
        "tofino-bin/include/tdi_tofino/*.h",
        "tofino-bin/include/tofino/bf_pal/*.h",
        "tofino-bin/include/tofino/pdfixed/*.h",
    ]),
    linkopts = [
        "-lpthread",
        "-lm",
        "-ldl",
    ],
    strip_include_prefix = "tofino-bin/include",
)

pkg_tar_with_symlinks(
    name = "tofino_library_files",
    srcs = glob([
        "tofino-bin/lib/bfshell_plugin_*.so*",
        "tofino-bin/lib/libavago.so*",
        "tofino-bin/lib/libbfutils.so*",
        "tofino-bin/lib/libclish.so*",
        "tofino-bin/lib/libdriver.so*",
        "tofino-bin/lib/libdru_sim.so*",
        "tofino-bin/lib/libpython3.*",
        "tofino-bin/lib/libtarget_sys.so*",
        "tofino-bin/lib/libtarget_utils.so*",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "tofino-bin",
)

pkg_tar_with_symlinks(
    name = "tofino_shareable_files",
    srcs = glob([
        "tofino-bin/share/bf_switchd/**",
        "tofino-bin/share/bf_rt_shared/**",
        "tofino-bin/share/bfsys/**",
        "tofino-bin/share/cli/xml/**",
        "tofino-bin/share/microp_fw/**",
        "tofino-bin/share/tofino_sds_fw/**",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "tofino-bin",
)

pkg_tar(
    name = "tofino_kernel_module",
    srcs = glob(["tofino-bin/lib/modules/**/*.ko"]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "tofino-bin",
)

# This string setting is templated with the correct version string by reading
# the $SDE_INSTALL/share/VERSION file. Then one of the config settings below
# will match and can be used with select().
string_setting(
    name = "sde_version_setting",
    build_setting_default = "{SDE_VERSION}",
)

config_setting(
    name = "sde_version_9.11.0",
    flag_values = {
        ":sde_version_setting": "9.11.0",
    },
)
