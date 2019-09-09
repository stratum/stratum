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

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

load("//bazel/rules:p4c_build_defs.bzl", "p4_bmv2_compile")
load("//stratum/pipelines/ptf:ptf_exec.bzl", "ptf_exec")

p4_bmv2_compile(
    name = "p4_loopback",
    src = "loopback.p4",
    hdrs = ["@com_github_p4lang_p4c//:p4include/core.p4",
            "@com_github_p4lang_p4c//:p4include/v1model.p4"],
    out_p4_info = "p4c-out/bmv2/loopback.p4info",
    out_p4_pipeline_json = "p4c-out/bmv2/loopback.json",
)

filegroup(
    name = "p4_loopback_test_files",
    srcs = ["ptf/loopback.py",
            "p4c-out/bmv2/loopback.p4info",
            "p4c-out/bmv2/loopback.json"]
)

ptf_exec(
    name = "p4_loopback_test",
    device = "stratum-bmv2",
    pipeline_name = "loopback",
    data = ["p4_loopback_test_files",
            "p4_loopback",
            "//stratum/hal/bin/bmv2:stratum_bmv2"]
)

ptf_exec(
    name = "p4_loopback_pipeline",
    device = "stratum-bmv2",
    pipeline_name = "loopback",
    data = ["p4_loopback_test_files",
            "p4_loopback"],
    skip_test = True,
    skip_bmv2_start = True,
)


