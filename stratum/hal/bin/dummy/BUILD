package(
    default_visibility = ["//visibility:public"],
)

load("//bazel:rules.bzl", "stratum_cc_binary", "HOST_ARCHES")

py_binary(
    name = "cli",
    srcs = ["cli.py"],
    deps = ["//stratum/hal/lib/dummy:dummy_test_py_proto",
            "//stratum/hal/lib/common:common_py_proto"]
)

py_binary(
    name = "port_counter_sim",
    srcs = ["port_counter_sim.py"],
    deps = ["//stratum/hal/bin/dummy:cli",
            "//stratum/hal/lib/dummy:dummy_test_py_proto",
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
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "//stratum/hal/lib/dummy:dummy_switch",
        "//stratum/hal/lib/dummy:dummy_phal",
        "//stratum/hal/lib/dummy:dummy_sdk",
    ],
)
