#
# Copyright 2019-present Open Networking Foundation
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

load(
    "//bazel:rules.bzl",
    "stratum_package",
    "stratum_cc_test",
)

package(default_visibility = ["//visibility:public"])


licenses(["notice"])  # Apache v2

filegroup(
    name = "configs",
    srcs = glob([
        "*"
    ]),
)

stratum_cc_test(
    name = "validate_configs",
    srcs = [
        "//stratum/hal/config/config_validator:main_src"
    ],
    deps = [
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/bcm:bcm_cc_proto",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "@com_google_absl//absl/strings:str_format",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_github_grpc_grpc//:grpc++",
    ],
    data = [
        ":configs"
    ],
    defines = [
        "PLATFORM=x86-64-dell-z9100-c2538-r0",
        "BCM_TARGET",
    ]
)