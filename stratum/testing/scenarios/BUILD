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

load("//stratum/testing/cdlang:cdlang.bzl", "cc_cdlang_library")

cc_cdlang_library(
    name = "scenarios",
    testonly = True,
    srcs = glob(["*.cdlang"]),
    libs = [
        "lib.cc.tmpl",
    ],
    template = "scenarios.cc.tmpl",
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status",
    ],
)

cc_test(
    name = "scenarios_test",
    testonly = 1,
    local = 1,
    deps = [
        ":scenarios",
        ":test_main",
    ],
)

cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "@com_google_googletest//:gtest",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)
