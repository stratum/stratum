# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

# load("@build_stack_rules_proto//python:python_grpc_library.bzl", "python_grpc_library")
load(":scapy_exec.bzl", "scapy_exec")

exports_files(["LICENSE"])

exports_files(["requirements.txt"])

package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "ptf_exec_files",
    srcs = ["ptf_exec.py"]
)

filegroup(
    name = "scapy_exec_files",
    srcs = ["scapy_exec.py"]
)

# python_grpc_library(
#     name = "ptf_py_lib",
#     deps = ["@com_google_googleapis//google/rpc:code_proto"],
# )

scapy_exec(
    name = "scapy"
    )
