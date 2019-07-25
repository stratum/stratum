#
# Copyright 2019 NoviFlow Inc.
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

load("@ptf_deps//:requirements.bzl", ptf_requirement = "requirement")

def scapy_exec( name ):

    py_args = []
    py_args.append("--scapy-dir")
    py_args.append(ptf_requirement("scapy")[1:-6])

    native.py_binary(
        name = name,
        srcs = ["//stratum/pipelines/ptf:scapy_exec_files"],
        main = "//stratum/pipelines/ptf:scapy_exec.py",
        args = py_args,
        deps = [ptf_requirement("scapy")],
    )
