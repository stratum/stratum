# Copyright 2020-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

load("@//bazel/rules:package_rule.bzl", "pkg_tar_with_symlinks")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@bazel_skylib//rules:common_settings.bzl", "string_setting")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "tdi_sde",
    srcs = glob([
        "lib/bfshell_plugin_*.so*",
        "lib/libbf_switchd_lib.so*",
        "lib/libclish.so*",
        "lib/libdpdk_infra.so*",
        "lib/libdriver.so*",
        "lib/libpython3*",
        "lib/libtarget_sys.so*",
        "lib/libtarget_utils.so*",
        "lib/libtdi_json_parser.so*",
        "lib/libtdi_pna.so*",
        "lib/libtdi.so*",
        # DPDK libs
        "lib/x86_64-linux-gnu/**/*.so*",
    ]),
    hdrs = glob([
        "include/bf_pal/*.h",
        "include/bf_rt/**/*.h",
        "include/bf_switchd/**/*.h",
        "include/bf_types/*.h",
        "include/cjson/*.h",
        "include/dvm/*.h",
        "include/lld/*.h",
        "include/osdep/*.h",
        "include/pipe_mgr/*.h",
        "include/port_mgr/**/*.h",
        "include/target-sys/**/*.h",
        "include/target-utils/**/*.h",
        "include/tdi_rt/**/*.h",
        "include/tdi_rt/**/*.hpp",
        "include/tdi/**/*.h",
        "include/tdi/**/*.hpp",
    ]),
    linkopts = [
        "-lpthread",
        "-lm",
        "-ldl",
    ],
    strip_include_prefix = "include",
)

# Runtime libraries
pkg_tar_with_symlinks(
    name = "tdi_library_files",
    srcs = glob([
        "lib/bfshell_plugin_*.so*",
        "lib/libbf_switchd_lib.so*",
        "lib/libclish.so*",
        "lib/libdpdk_infra.so*",
        "lib/libdriver.so*",
        "lib/libpython3*",
        "lib/libtarget_sys.so*",
        "lib/libtarget_utils.so*",
        "lib/libtdi_json_parser.so*",
        "lib/libtdi_pna.so*",
        "lib/libtdi.so*",
        # DPDK libs
        "lib/x86_64-linux-gnu/**/*.so*",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "local_tdi_bin",
)

pkg_tar_with_symlinks(
    name = "tdi_shareable_files",
    srcs = glob([
        "share/bf_rt_shared/**",
        "share/cli/xml/**",
        "share/target_sys/**",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "local_tdi_bin",
)

# FIXME: the rules below are not correctly working without templating, check if
#        still needed or remove them

# This string setting is templated with the correct version string by reading
# the $SDE_INSTALL/share/VERSION file. Then one of the config settings below
# will match and can be used with select().
string_setting(
    name = "sde_version_setting",
    build_setting_default = "{SDE_VERSION}",
)

config_setting(
    name = "sde_version_0.1.0",
    flag_values = {
        ":sde_version_setting": "0.1.0",
    },
)

# This string setting is also present in the Tofino TDI BUILD file. Depending on
# the backend selected in the SDE_INSTALL, the correct config setting will match
# and can be used with select().
string_setting(
    name = "tdi_backend_setting",
    build_setting_default = "dpdk",
)

# This setting is true.
config_setting(
    name = "tdi_backend_dpdk",
    flag_values = {
        ":tdi_backend_setting": "dpdk",
    },
)

# This setting is false.
config_setting(
    name = "tdi_backend_tofino",
    flag_values = {
        ":tdi_backend_setting": "tofino",
    },
)
