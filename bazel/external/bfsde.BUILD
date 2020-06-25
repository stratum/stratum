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
    srcs = [
        "barefoot-bin/lib/libavago.a",
        "barefoot-bin/lib/libbf_switchd_lib.a",
        "barefoot-bin/lib/libbfsys.so",
        "barefoot-bin/lib/libbfutils.a",
        "barefoot-bin/lib/libdriver.so",
        "barefoot-bin/lib/libpython3.4m.so",
    ],
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
    includes = ["barefoot-bin/include"],
    linkopts = [
        "-lpthread",
        "-lm",
        "-ldl",
    ],
)

pkg_tar(
    name = "bf_library_files",
    srcs = [
        "barefoot-bin/lib/libavago.so",
        "barefoot-bin/lib/libbfsys.so",
        "barefoot-bin/lib/libdriver.so",
        "barefoot-bin/lib/libdru_sim.so",
        "barefoot-bin/lib/libpython3.4m.so",
    ],
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "barefoot-bin",
)

pkg_tar_with_symlinks(
    name = "bf_shareable_files",
    srcs = glob(["barefoot-bin/share/microp_fw/**"]) + glob([
        "barefoot-bin/share/bfsys/**",
    ]) + glob([
        "barefoot-bin/share/tofino_sds_fw/**",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "barefoot-bin",
)

pkg_tar(
    name = "kernel_module",
    srcs = glob(["barefoot-bin/lib/modules/*.ko"]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "barefoot-bin",
)
