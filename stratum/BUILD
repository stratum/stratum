# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_package",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

config_setting(
    name = "stratum_ppc",
    values = {
        "crosstool_top": "//unsupported_toolchains/crosstoolng_powerpc32_8540",
        "copt": "-DSTRATUM_ARCH_PPC",
    },
)

config_setting(
    name = "stratum_x86",
    values = {
        "crosstool_top": "//unsupported_toolchains/crosstoolng_planetx86",
        "copt": "-DSTRATUM_ARCH_X86",
    },
)

config_setting(
    name = "llvm_compiler",
    flag_values = {"@bazel_tools//tools/cpp:compiler": "clang"},
)

config_setting(
    name = "gcc_compiler",
    flag_values = {"@bazel_tools//tools/cpp:compiler": "gcc"},
)

stratum_package(
    name = "stratum_package_standalone",
    deps = [
        "//stratum/hal/bin/bcm/standalone:stratum_hal_pkg",
        "//stratum/hal/stub/embedded:stratum_stub_pkg",
    ],
)
