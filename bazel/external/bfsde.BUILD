# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@//bazel/rules:package_rule.bzl", "pkg_tar_with_symlinks")
load("@rules_proto//proto:defs.bzl", "proto_library")
load(
     "@rules_cc//cc:defs.bzl",
     "cc_library",
     "cc_proto_library"
)
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
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
        "barefoot-bin/include/traffic_mgr/*.h",
        "barefoot-bin/include/tofino/bf_pal/*.h",
        "barefoot-bin/include/tofino/pdfixed/*.h",
    ]),
    strip_include_prefix = "barefoot-bin/include",
    linkopts = [
        "-lpthread",
        "-lm",
        "-ldl",
    ],
)

cc_library(
    name = "bfruntime_server_h",
    hdrs = glob([
        "barefoot-bin/include/bf_rt/proto/*.h",
    ]),
    strip_include_prefix = "barefoot-bin/include",
)

cc_library(
    name = "bfruntime_common",
    hdrs = glob([
        "barefoot-bin/src/bf_rt/bf_rt_common/*.hpp",
    ]),
    strip_include_prefix = "barefoot-bin/src/bf_rt",
    deps = [
        ":bfsde",
    ],
)

cc_library(
    name = "bfruntime_server",
    srcs = glob([
        "barefoot-bin/src/bf_rt/proto/*.cpp",
    ]),
    hdrs = glob([
        "barefoot-bin/src/bf_rt/proto/*.hpp",
    ]),
    deps = [
        ":bfsde",
        ":bfruntime_cc_proto",
        ":bfruntime_cc_grpc",
        ":bfruntime_common",
        ":bfruntime_server_h",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
    includes = [
        "barefoot-bin/src/bf_rt",
        "barefoot-bin/src/bf_rt/proto",
    ],
)

proto_library(
    name = "bfruntime_proto",
    srcs = ["barefoot-bin/src/bf_rt/proto/bfruntime.proto"],
    deps = ["@com_google_googleapis//google/rpc:status_proto"],
)

cc_proto_library(
    name = "bfruntime_cc_proto",
    deps = [":bfruntime_proto"],
)

cc_grpc_library(
    name = "bfruntime_cc_grpc",
    grpc_only = True,
    srcs = [":bfruntime_proto"],
    deps = [":bfruntime_cc_proto"],
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
    srcs = glob([
        "barefoot-bin/share/microp_fw/**",
        "barefoot-bin/share/bfsys/**",
        "barefoot-bin/share/tofino_sds_fw/**",
        "barefoot-bin/share/bf_rt_shared/**",
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
