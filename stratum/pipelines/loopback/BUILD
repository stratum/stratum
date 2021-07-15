# Copyright 2019 NoviFlow Inc.
# SPDX-License-Identifier: Apache-2.0

load("//bazel/rules:p4c_build_defs.bzl", "p4_bmv2_compile")
load("//stratum/pipelines/ptf:ptf_exec.bzl", "ptf_exec")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

p4_bmv2_compile(
    name = "p4_loopback",
    src = "loopback.p4",
    hdrs = [
        "@com_github_p4lang_p4c//:p4include/core.p4",
        "@com_github_p4lang_p4c//:p4include/v1model.p4",
    ],
    out_p4_info = "p4c-out/bmv2/loopback.p4info",
    out_p4_pipeline_json = "p4c-out/bmv2/loopback.json",
)

filegroup(
    name = "p4_loopback_test_files",
    srcs = [
        "p4c-out/bmv2/loopback.json",
        "p4c-out/bmv2/loopback.p4info",
        "ptf/loopback.py",
    ],
)

ptf_exec(
    name = "p4_loopback_test",
    data = [
        "p4_loopback",
        "p4_loopback_test_files",
        "//stratum/hal/bin/bmv2:stratum_bmv2",
    ],
    device = "stratum-bmv2",
    pipeline_name = "loopback",
)

ptf_exec(
    name = "p4_loopback_pipeline",
    data = [
        "p4_loopback",
        "p4_loopback_test_files",
    ],
    device = "stratum-bmv2",
    pipeline_name = "loopback",
    skip_bmv2_start = True,
    skip_test = True,
)
