# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@//bazel/rules:package_rule.bzl", "pkg_tar_with_symlinks")
load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(
    default_visibility = ["//visibility:public"],
)

# trick to export headers in a convenient way
cc_library(
    name = "bf_headers",
    hdrs = glob([
        "barefoot-bin/include/bf_switchd/*.h",
        "barefoot-bin/include/bfsys/**/*.h",
        "barefoot-bin/include/bfutils/**/*.h",
        "barefoot-bin/include/bf_types/*.h",
        "barefoot-bin/include/dvm/*.h",
        "barefoot-bin/include/mc_mgr/*.h",
        "barefoot-bin/include/port_mgr/*.h",
        "barefoot-bin/include/tofino/bf_pal/*.h",
    ]),
    includes = ["barefoot-bin/include"],
)

cc_import(
    name = "bf_switchd",
    hdrs = [],  # see cc_library rule above
    shared_library = "barefoot-bin/lib/libbf_switchd_lib.so",
    # If alwayslink is turned on, libbf_switchd_lib.so will be forcely linked
    # into any binary that depends on it.
    alwayslink = 1,
)

cc_import(
    name = "bf_drivers",
    hdrs = [],  # see cc_library rule above
    shared_library = "barefoot-bin/lib/libdriver.so",
    alwayslink = 1,
)

cc_import(
    name = "bf_sys",
    hdrs = [],  # see cc_library rule above
    shared_library = "barefoot-bin/lib/libbfsys.so",
    alwayslink = 1,
)

cc_import(
    name = "bf_utils",
    hdrs = [],  # see cc_library rule above
    shared_library = "barefoot-bin/lib/libbfutils.so",
    alwayslink = 1,
)

pkg_tar(
    name = "bf_library_files",
    srcs = glob(["barefoot-bin/lib/*.so"]),
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
