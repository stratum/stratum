# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(":scapy_exec.bzl", "scapy_exec")

licenses(["notice"])  # Apache v2

exports_files(["LICENSE"])

exports_files(["requirements.txt"])

package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "ptf_exec_files",
    srcs = ["ptf_exec.py"],
)

filegroup(
    name = "scapy_exec_files",
    srcs = ["scapy_exec.py"],
)


scapy_exec(
    name = "scapy",
)
