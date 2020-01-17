#
# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

licenses(["notice"])  # Apache v2

load("@build_stack_rules_proto//python:python_grpc_library.bzl", "python_grpc_library")
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

python_grpc_library(
    name = "ptf_py_lib",
    deps = ["@com_google_googleapis//google/rpc:code_proto"],
)

scapy_exec(
    name = "scapy"
    )
