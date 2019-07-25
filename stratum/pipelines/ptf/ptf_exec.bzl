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

load("@grpc_py_deps//:requirements.bzl", grpc_all_requirements = "all_requirements", grpc_requirement = "requirement")
load("@ptf_deps//:requirements.bzl", ptf_all_requirements = "all_requirements", ptf_requirement = "requirement")

def ptf_exec( name, device, pipeline_name, data=None, skip_test=False, skip_bmv2_start=False ):

    py_data = []
    for x in data:
        py_data.append(x)
    py_data.append("@com_github_opennetworkinglab_fabric_p4test//:fabric_p4test_stratum_p4_test_files")

    py_args = []
    py_args.append(" --device ")
    py_args.append(device)
    py_args.append(" --pipeline-name ")
    py_args.append(pipeline_name)
    if skip_test == True:
        py_args.append(" --skip-test")
    if skip_bmv2_start == True:
        py_args.append(" --skip-bmv2-start")

    # Provide directories in the form pypi__<module>_<version>, without the "@" prefix and the "//:pkg" suffix
    py_args.append(" --protobuf-dir ")
    py_args.append(grpc_requirement("protobuf")[1:-6])
    py_args.append(" --ptf-dir ")
    py_args.append(ptf_requirement("ptf")[1:-6])
    py_args.append(" --scapy-dir")
    py_args.append(ptf_requirement("scapy")[1:-6])

    native.py_binary(
        name = name,
        srcs = ["//stratum/pipelines/ptf:ptf_exec_files"],
        main = "//stratum/pipelines/ptf:ptf_exec.py",
        args = py_args,
        deps = ["@com_github_p4lang_p4runtime//:p4runtime_py_grpc",
                "//stratum/pipelines/ptf:ptf_py_lib"] +
                depset(grpc_all_requirements).to_list() +
                depset(ptf_all_requirements).to_list(),
        data = py_data
    )
