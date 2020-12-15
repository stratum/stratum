# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "headers",
    hdrs = glob([
        "include/**/*.h",
        "src/gpl-modules/include/**/*.h",
        "src/gpl-modules/systems/bde/linux/include/**/*.h",
    ]),
    includes = [
        "include",
        "src/gpl-modules/include",
        "src/gpl-modules/systems/bde/linux/include",
    ],
)

cc_library(
    name = "libsdk",
    srcs = [
        "lib/x86-64/libopennsa.a",
        "src/diag/config_init_defaults.c",
        "src/diag/version.c",
    ],
    linkopts = [
        "-lpthread",
        "-lm",
        "-lrt",
    ],
    linkstatic = True,
    deps = [":headers"],
)

filegroup(
    name = "kernel_modules",
    srcs = [
        "src/gpl-modules/systems/linux/user/x86-smp_generic_64-2_6/linux-bcm-knet.ko",
        "src/gpl-modules/systems/linux/user/x86-smp_generic_64-2_6/linux-kernel-bde.ko",
        "src/gpl-modules/systems/linux/user/x86-smp_generic_64-2_6/linux-user-bde.ko",
    ],
)
