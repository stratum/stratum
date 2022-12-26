# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@//bazel/rules:package_rule.bzl", "pkg_tar_with_symlinks")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("@bazel_skylib//rules:common_settings.bzl", "string_setting")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "bfsde",
    srcs = glob([
        "barefoot-bin/lib/bfshell_plugin*.so*",
        "barefoot-bin/lib/libavago.so*",
        "barefoot-bin/lib/libbf_switchd_lib.a*",
        "barefoot-bin/lib/libbfsys.so*",
        "barefoot-bin/lib/libbfutils.so*",
        "barefoot-bin/lib/libclish.so*",
        "barefoot-bin/lib/libdriver.so*",
        "barefoot-bin/lib/libpython3*",
        # target libraries from p4lang (was libbfsys and libbfutils before 9.9.0)
        "barefoot-bin/lib/libtarget_sys.so*",
        "barefoot-bin/lib/libtarget_utils.so*",
    ]),
    hdrs = glob([
        "barefoot-bin/include/bf_rt/*.h",
        "barefoot-bin/include/bf_rt/*.hpp",
        "barefoot-bin/include/bf_switchd/*.h",
        "barefoot-bin/include/bf_types/*.h",
        "barefoot-bin/include/bfsys/**/*.h",
        "barefoot-bin/include/bfutils/**/*.h",
        "barefoot-bin/include/dvm/*.h",
        "barefoot-bin/include/lld/*.h",
        "barefoot-bin/include/mc_mgr/*.h",
        "barefoot-bin/include/pipe_mgr/*.h",
        "barefoot-bin/include/pkt_mgr/*.h",
        "barefoot-bin/include/port_mgr/*.h",
        "barefoot-bin/include/tofino/bf_pal/*.h",
        "barefoot-bin/include/tofino/pdfixed/*.h",
        "barefoot-bin/include/traffic_mgr/*.h",
        "barefoot-bin/include/target-sys/**/*.h",
        "barefoot-bin/include/target-utils/**/*.h",
    ]),
    linkopts = [
        "-lpthread",
        "-lm",
        "-ldl",
    ],
    strip_include_prefix = "barefoot-bin/include",
)

pkg_tar_with_symlinks(
    name = "bf_library_files",
    # Using a wildcard glob here to match the shared libraries makes this rule
    # more generic than a normal source list, as it does not require that all
    # targets are present, which is the case for non-BSP SDE builds. Extending
    # this rule for additional BSP platforms is as easy as adding more matches
    # to the list.
    srcs = glob([
        "barefoot-bin/lib/bfshell_plugin_*.so*",
        "barefoot-bin/lib/libavago.so*",
        "barefoot-bin/lib/libbfsys.so*",
        "barefoot-bin/lib/libbfutils.so*",
        "barefoot-bin/lib/libclish.so*",
        "barefoot-bin/lib/libdriver.so*",
        "barefoot-bin/lib/libdru_sim.so*",
        "barefoot-bin/lib/libpython3*",
        # General BSP libraries.
        "barefoot-bin/lib/libpltfm_driver.so*",
        "barefoot-bin/lib/libpltfm_mgr.so*",
        # BSP libraries for Edgecore Wedge100bf series.
        "barefoot-bin/lib/libacctonbf_driver.so*",
        "barefoot-bin/lib/libtcl_server.so*",
        # target libraries from p4lang (was libbfsys and libbfutils before 9.9.0)
        "barefoot-bin/lib/libtarget_sys.so*",
        "barefoot-bin/lib/libtarget_utils.so*",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "barefoot-bin",
)

pkg_tar_with_symlinks(
    name = "bf_shareable_files",
    srcs = glob([
        "barefoot-bin/share/bf_rt_shared/**",
        "barefoot-bin/share/bfsys/**",
        "barefoot-bin/share/bf_switchd/**",
        "barefoot-bin/share/cli/xml/**",
        "barefoot-bin/share/microp_fw/**",
        "barefoot-bin/share/tofino_sds_fw/**",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "barefoot-bin",
)

pkg_tar(
    name = "kernel_module",
    srcs = glob(["barefoot-bin/lib/modules/**/*.ko"]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "barefoot-bin",
)

pkg_tar(
    name = "bf_binary_files",
    srcs = glob([
        "barefoot-bin/bin/credo_firmware.bin*",  # firmware for retimers in the 65x
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "barefoot-bin",
)

# This string setting is templated with the correct version string by reading
# the $SDE_INSTALL/share/VERSION file. Then one of the config settings below
# will match and can be used with select().
string_setting(
    name = "sde_version_setting",
    build_setting_default = "{SDE_VERSION}",
)

config_setting(
    name = "sde_version_9.7.0",
    flag_values = {
        ":sde_version_setting": "9.7.0",
    },
)

config_setting(
    name = "sde_version_9.7.1",
    flag_values = {
        ":sde_version_setting": "9.7.1",
    },
)

config_setting(
    name = "sde_version_9.7.2",
    flag_values = {
        ":sde_version_setting": "9.7.2",
    },
)

config_setting(
    name = "sde_version_9.8.0",
    flag_values = {
        ":sde_version_setting": "9.8.0",
    },
)

config_setting(
    name = "sde_version_9.9.0",
    flag_values = {
        ":sde_version_setting": "9.9.0",
    },
)

config_setting(
    name = "sde_version_9.10.0",
    flag_values = {
        ":sde_version_setting": "9.10.0",
    },
)
