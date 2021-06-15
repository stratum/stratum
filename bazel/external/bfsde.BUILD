# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@//bazel/rules:package_rule.bzl", "pkg_tar_with_symlinks")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("@bazel_skylib//rules:common_settings.bzl", "bool_setting", "string_setting")

package(
    default_visibility = ["//visibility:public"],
)

# Contains all the BSP libraries, if the SDE was built with it.
bsp_srcs = select({
    ":sde_with_bsp": glob([
        "barefoot-bin/lib/libacctonbf_driver.so*",
        "barefoot-bin/lib/libpltfm_driver.so*",
        "barefoot-bin/lib/libpltfm_mgr.so*",
        "barefoot-bin/lib/libtcl_server.so*",
    ]),
    "//conditions:default": [],
})

# Contains all the BSP headers, if the SDE was built with it.
bsp_hdrs = select({
    ":sde_with_bsp": glob([
        "barefoot-bin/include/bf_led/*.h",
        "barefoot-bin/include/bf_pltfm/**/*.h",
        "barefoot-bin/include/bf_pltfm_types/*.h",
    ]),
    "//conditions:default": [],
})

cc_library(
    name = "bfsde",
    srcs = glob([
        "barefoot-bin/lib/libavago.so*",
        "barefoot-bin/lib/libbfsys.so*",
        "barefoot-bin/lib/libbfutils.so*",
        "barefoot-bin/lib/libdriver.so*",
        "barefoot-bin/lib/libpython3.4m.so*",
    ]) + ["barefoot-bin/lib/libbf_switchd_lib.a"] + bsp_srcs,
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
    ]) + bsp_hdrs,
    linkopts = [
        "-lpthread",
        "-lm",
        "-ldl",
    ],
    strip_include_prefix = "barefoot-bin/include",
    deps = [
        # TODO(bocon): PI needed when linking libdriver.so if/when pi is
        # enabled when building bf-drivers. This shouldn't hurt, but can
        # be excluded if/when PI is removed from the SDE build options.
        "@//stratum/hal/lib/pi:pi_bf",
    ],
)

pkg_tar_with_symlinks(
    name = "bf_library_files",
    srcs = glob([
        "barefoot-bin/lib/bfshell_plugin_*.so*",
        "barefoot-bin/lib/libavago.so*",
        "barefoot-bin/lib/libbfsys.so*",
        "barefoot-bin/lib/libbfutils.so*",
        "barefoot-bin/lib/libdriver.so*",
        "barefoot-bin/lib/libdru_sim.so*",
        "barefoot-bin/lib/libpython3.4m.so*",
    ]) + bsp_srcs,
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "barefoot-bin",
)

pkg_tar_with_symlinks(
    name = "bf_shareable_files",
    srcs = glob([
        "barefoot-bin/share/bf_rt_shared/**",
        "barefoot-bin/share/bfsys/**",
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

# This string setting is templated with the correct version string by reading
# the $SDE_INSTALL/share/VERSION file. Then one of the config settings below
# will match and can be used with select().
string_setting(
    name = "sde_version_setting",
    build_setting_default = "{SDE_VERSION}",
)

# This bool setting is templated with whether the SDE was built with BSP support
# or not.
bool_setting(
    name = "sde_with_bsp_setting",
    build_setting_default = {SDE_WITH_BSP},
)

config_setting(
    name = "sde_version_9.2.0",
    flag_values = {
        ":sde_version_setting": "9.2.0",
    },
)

config_setting(
    name = "sde_version_9.3.0",
    flag_values = {
        ":sde_version_setting": "9.3.0",
    },
)

config_setting(
    name = "sde_version_9.3.1",
    flag_values = {
        ":sde_version_setting": "9.3.1",
    },
)

config_setting(
    name = "sde_version_9.3.2",
    flag_values = {
        ":sde_version_setting": "9.3.2",
    },
)

config_setting(
    name = "sde_version_9.4.0",
    flag_values = {
        ":sde_version_setting": "9.4.0",
    },
)

config_setting(
    name = "sde_version_9.5.0",
    flag_values = {
        ":sde_version_setting": "9.5.0",
    },
)

config_setting(
    name = "sde_with_bsp",
    flag_values = {
        ":sde_with_bsp_setting": "True",
    },
)

config_setting(
    name = "sde_without_bsp",
    flag_values = {
        ":sde_with_bsp_setting": "False",
    },
)
