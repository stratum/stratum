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

load(
    "//bazel:rules.bzl",
    "stratum_cc_test",
)

def platform_config_test(platform, bcm_target=False):
    defines = ["PLATFORM=" + platform]
    if bcm_target:
        defines.append("BCM_TARGET")

    stratum_cc_test(
        name = "validate_configs_" + platform,
        srcs = [
            "config_validator_main.cc"
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
            "@com_google_googletest//:gtest_main",
            "@com_github_grpc_grpc//:grpc++",
        ],
        data = native.glob([platform + "/*.pb.txt"]),
        defines = defines
    )
