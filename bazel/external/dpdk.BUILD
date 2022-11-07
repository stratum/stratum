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
        "tdi-bin/lib/bfshell_plugin_*.so*",
        "tdi-bin/lib/libbf_switchd_lib.so*",
        "tdi-bin/lib/libclish.so*",
        "tdi-bin/lib/libdpdk_infra.so*",
        "tdi-bin/lib/libdriver.so*",
        "tdi-bin/lib/libpython3*",
        "tdi-bin/lib/libtarget_sys.so*",
        "tdi-bin/lib/libtarget_utils.so*",
        "tdi-bin/lib/libtdi_json_parser.so*",
        "tdi-bin/lib/libtdi_pna.so*",
        "tdi-bin/lib/libtdi.so*",
        # DPDK libs
        "tdi-bin/lib/x86_64-linux-gnu/dpdk/pmds-22.2/*.so*",
        "tdi-bin/lib/x86_64-linux-gnu/librte_*.so*",
    ]),
    hdrs = glob([
        "tdi-bin/include/bf_pal/*.h",
        "tdi-bin/include/bf_rt/**/*.h",
        "tdi-bin/include/bf_switchd/**/*.h",
        "tdi-bin/include/bf_types/*.h",
        "tdi-bin/include/cjson/*.h",
        "tdi-bin/include/dvm/*.h",
        "tdi-bin/include/lld/*.h",
        "tdi-bin/include/osdep/*.h",
        "tdi-bin/include/pipe_mgr/*.h",
        "tdi-bin/include/port_mgr/**/*.h",
        "tdi-bin/include/target-sys/**/*.h",
        "tdi-bin/include/target-utils/**/*.h",
        "tdi-bin/include/tdi_rt/**/*.h",
        "tdi-bin/include/tdi_rt/**/*.hpp",
        "tdi-bin/include/tdi/**/*.h",
        "tdi-bin/include/tdi/**/*.hpp",
    ]),
    linkopts = [
        "-lpthread",
        "-lm",
        "-ldl",
    ],
    strip_include_prefix = "tdi-bin/include",
)

# Runtime libraries
pkg_tar_with_symlinks(
    name = "tdi_library_files",
    srcs = glob([
        "tdi-bin/lib/bfshell_plugin_*.so*",
        "tdi-bin/lib/libbf_switchd_lib.so*",
        "tdi-bin/lib/libclish.so*",
        "tdi-bin/lib/libdpdk_infra.so*",
        "tdi-bin/lib/libdriver.so*",
        "tdi-bin/lib/libpython3*",
        "tdi-bin/lib/libtarget_sys.so*",
        "tdi-bin/lib/libtarget_utils.so*",
        "tdi-bin/lib/libtdi_json_parser.so*",
        "tdi-bin/lib/libtdi_pna.so*",
        "tdi-bin/lib/libtdi.so*",
        # DPDK libs
        "tdi-bin/lib/x86_64-linux-gnu/dpdk/pmds-22.2/*.so*",
        "tdi-bin/lib/x86_64-linux-gnu/librte_*.so*",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "tdi-bin",
)

pkg_tar_with_symlinks(
    name = "tdi_shareable_files",
    srcs = glob([
        "tdi-bin/share/bf_rt_shared/**",
        "tdi-bin/share/cli/xml/**",
        "tdi-bin/share/target_sys/**",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "tdi-bin",
)

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
