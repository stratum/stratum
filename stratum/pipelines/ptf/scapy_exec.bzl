# Copyright 2019 NoviFlow Inc.
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_python//python:defs.bzl", "py_binary")
load("@ptf_deps//:requirements.bzl", ptf_requirement = "requirement")

def scapy_exec(name):
    py_args = []
    py_args.append("--scapy-dir")
    py_args.append(ptf_requirement("scapy")[1:-6])
    py_binary(
        name = name,
        srcs = ["//stratum/pipelines/ptf:scapy_exec_files"],
        main = "//stratum/pipelines/ptf:scapy_exec.py",
        args = py_args,
        deps = [ptf_requirement("scapy")],
    )
