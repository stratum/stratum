# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("//bazel/rules:p4c_build_defs.bzl", "p4_bmv2_compile")
load("//stratum/p4c_backends/common:build_defs.bzl", "p4_fpm_compile")

licenses(["notice"])  # Apache v2

# Build the main P4 pipeline for Broadcom switches.

filegroup(
    name = "annotation_map_files",
    srcs = [
        "field_map.pb.txt",
        "table_map.pb.txt",
    ],
)

p4_fpm_compile(
    name = "main_fpm",
    src = "main.p4",
    hdrs = [],
    annotation_maps = [":annotation_map_files"],
    out_p4_info = "fpm/main.p4info",
    out_p4_pipeline_binary = "fpm/main.pb.bin",
    out_p4_pipeline_text = "fpm/main.pb.txt",
)

p4_bmv2_compile(
    name = "main_bmv2",
    src = "main.p4",
    hdrs = [],
    out_p4_info = "bmv2/main.p4info",
    out_p4_pipeline_json = "bmv2/main.json",
)

pkg_tar(
    name = "main_pipelines",
    srcs = [
        "bmv2/main.json",
        "bmv2/main.p4info",
        "fpm/main.p4info",
        "fpm/main.pb.bin",
        "fpm/main.pb.txt",
    ],
    extension = "tar.gz",
    mode = "0644",
    strip_prefix = ".",
)
