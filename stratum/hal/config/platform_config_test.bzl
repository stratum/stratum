#
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
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
