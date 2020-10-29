# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@//bazel/rules:package_rule.bzl", "pkg_tar_with_symlinks")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "bfsde",
    srcs = glob([
        "barefoot-bin/lib/libavago.so*",
        "barefoot-bin/lib/libbf_switchd_lib.so*",
        "barefoot-bin/lib/libbfsys.so*",
        "barefoot-bin/lib/libbfutils.so*",
        "barefoot-bin/lib/libdriver.so*",
        "barefoot-bin/lib/libpython3.4m.so*",
    ]),
    hdrs = glob([
        "barefoot-bin/include/bf_switchd/*.h",
        "barefoot-bin/include/bfsys/**/*.h",
        "barefoot-bin/include/bfutils/**/*.h",
        "barefoot-bin/include/bf_types/*.h",
        "barefoot-bin/include/dvm/*.h",
        "barefoot-bin/include/mc_mgr/*.h",
        "barefoot-bin/include/port_mgr/*.h",
        "barefoot-bin/include/pipe_mgr/*.h",
        "barefoot-bin/include/tofino/bf_pal/*.h",
        "barefoot-bin/include/tofino/pdfixed/*.h",
    ]),
    deps = [
        # TODO(bocon): PI needed when linking libdriver.so if/when pi is
        # enabled when building bf-drivers. This shouldn't hurt, but can
        # be excluded if/when PI is removed from the SDE build options.
        "@//stratum/hal/lib/pi:pi_bf",
    ],
    linkopts = [
        "-lpthread",
        "-lm",
        "-ldl",
    ],
    strip_include_prefix = "barefoot-bin/include",
)

pkg_tar_with_symlinks(
    name = "bf_library_files",
    srcs = glob([
        "barefoot-bin/lib/libavago.so*",
        "barefoot-bin/lib/libbf_switchd_lib.so*",
        "barefoot-bin/lib/libbfsys.so*",
        "barefoot-bin/lib/libbfutils.so*",
        "barefoot-bin/lib/libdriver.so*",
        "barefoot-bin/lib/libdru_sim.so*",
        "barefoot-bin/lib/libpiall.so*",
        "barefoot-bin/lib/libpython3.4m.so*",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "barefoot-bin",
)

pkg_tar_with_symlinks(
    name = "bf_shareable_files",
    srcs = glob([
        "barefoot-bin/share/microp_fw/**",
        "barefoot-bin/share/bfsys/**",
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
