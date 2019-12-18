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
load("//bazel:rules.bzl", "stratum_cc_binary", "HOST_ARCHES")
load(
    "@grpc_py_deps//:requirements.bzl",
    grpc_all_requirements = "all_requirements",
    grpc_requirement = "requirement"
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

# TODO(Yi) Move to CC
py_binary(
    name = "cli",
    srcs = ["cli.py"],
    deps = [
        "//stratum/hal/lib/dummy:dummy_test_py_grpc",
        "//stratum/hal/lib/common:common_py_proto",
    ] + depset(grpc_all_requirements).to_list()
)

py_binary(
    name = "port_counter_sim",
    srcs = ["port_counter_sim.py"],
    deps = ["//stratum/hal/bin/dummy:cli",
            "//stratum/hal/lib/dummy:dummy_test_py_grpc",
            "//stratum/hal/lib/common:common_py_proto"]
)

stratum_cc_binary(
    name = "stratum_dummy",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    deps = [
        "@com_github_google_glog//:glog",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/phal:phal",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "//stratum/hal/lib/dummy:dummy_switch",
        "//stratum/hal/lib/dummy:dummy_box",
    ],
)
