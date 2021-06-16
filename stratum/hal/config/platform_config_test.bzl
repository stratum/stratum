#
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#

load(
    "//bazel:rules.bzl",
    "stratum_cc_test",
)

def platform_config_test(
        platform,
        bcm_target = False,
        sim_target = False,
        tofino_target = False):
    defines = ["PLATFORM=" + platform]
    data = [platform + "/chassis_config.pb.txt"]

    if bcm_target:
        defines.append("BCM_TARGET")
        data.append(platform + "/phal_config.pb.txt")
        data.append(platform + "/base_bcm_chassis_map.pb.txt")

    if sim_target:
        defines.append("SIM_TARGET")

    if tofino_target:
        defines.append("TOFINO_TARGET")
        data.append(platform + "/phal_config.pb.txt")
        data.append(platform + "/port_map.json")

    stratum_cc_test(
        name = "validate_configs_" + platform,
        srcs = [
            "config_validator_main.cc",
        ],
        deps = [
            "//stratum/glue/status:status_test_util",
            "//stratum/hal/lib/bcm:bcm_cc_proto",
            "//stratum/hal/lib/common:common_cc_proto",
            "//stratum/hal/lib/phal:phal_cc_proto",
            "//stratum/lib:utils",
            "//stratum/lib/test_utils:matchers",
            "@com_github_google_glog//:glog",
            "@com_github_grpc_grpc//:grpc++",
            "@com_github_nlohmann_json//:json",
            "@com_google_absl//absl/strings:str_format",
            "@com_google_googletest//:gtest_main",
            "@com_google_googletest//:gtest",
        ],
        data = data,
        defines = defines,
    )
