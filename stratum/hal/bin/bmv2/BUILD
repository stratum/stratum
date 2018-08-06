package(
    default_visibility = ["//visibility:public"],
)

load("//bazel:rules.bzl", "stratum_cc_binary", "HOST_ARCHES")

stratum_cc_binary(
    name = "stratum_bmv2",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    deps = [
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_PI//proto/frontend:pifeproto",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/bmv2:bmv2_switch",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/p4:p4_runtime_real",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/hal/lib/pi:pi_node",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "@local_bmv2_bin//:bmv2_headers",
        "@local_bmv2_bin//:bmv2_simple_switch",
        "@local_bmv2_bin//:bmv2_pi",
    ],
    data = ["dummy.json"],
)
