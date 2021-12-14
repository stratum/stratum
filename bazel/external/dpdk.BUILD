# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@//bazel/rules:package_rule.bzl", "pkg_tar_with_symlinks")

package(
    default_visibility = ["//visibility:public"],
)

# TODO(max): DPDK static linking is broken:
# MBUF: error setting mempool handler
# Let's go with shared libs for now.

# DPDK static libraries need -Wl,-whole-archive to properly link. Bazel has no
# way to specify this for externally imported libraries. Therefore, we merge
# them all into one big static library by hand here. The first step creates a
# "thin" library, the second step the desired one.
genrule(
    name = "dpdk_fat",
    srcs = glob(["dpdk-install/lib/*.a"]),
    outs = ["libdpdk_fat.a"],
    cmd = "ar cqT libdpdk_fat_thin.a $(SRCS); " +
          "echo -e 'create $@\naddlib libdpdk_fat_thin.a\nsave\nend' | ar -M",
    visibility = ["//visibility:private"],
)

cc_library(
    name = "dpdk",
    srcs = [":dpdk_fat"],
    hdrs = glob([
        "dpdk-install/include/*.h",
        "dpdk-install/include/generic/*.h",
    ]),
    copts = [
        "-Wno-error=implicit-fallthrough",
    ],
    linkopts = [
        "-lnuma",
        "-ldl",
        "-lpcap",
    ],
    linkstatic = True,
    strip_include_prefix = "dpdk-install/include",
    alwayslink = 1,
)

cc_library(
    name = "dpdk_shared",
    srcs = glob(["dpdk-install/lib/*.so"]),
    hdrs = glob([
        "dpdk-install/include/*.h",
        "dpdk-install/include/generic/*.h",
    ]),
    copts = [
        "-Wno-error=implicit-fallthrough",
    ],
    linkopts = [
        "-lnuma",
        "-ldl",
        "-lpcap",
    ],
    strip_include_prefix = "dpdk-install/include",
    alwayslink = 1,
)

pkg_tar_with_symlinks(
    name = "dpdk_shared_library_files",
    srcs = glob([
        "dpdk-install/lib/**/*.so.*",
    ]),
    mode = "0644",
    package_dir = "/usr",
    strip_prefix = "dpdk-install",
)
