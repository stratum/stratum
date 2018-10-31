load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_package",
    "stratum_cc_proto_library",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_proto_library(
    name = "procmon_proto",
    srcs = ["procmon.proto"],
    with_grpc = True,
)

stratum_cc_library(
    name = "procmon",
    srcs = ["procmon.cc"],
    hdrs = ["procmon.h"],
    deps = [
        ":procmon_proto",
        "@com_github_google_glog//:glog",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:status",
    ],
)

stratum_cc_library(
    name = "procmon_service_impl",
    srcs = ["procmon_service_impl.cc"],
    hdrs = ["procmon_service_impl.h"],
    deps = [
        ":procmon_grpc_proto",
        "@com_google_grpc//:grpc++",
    ],
)

stratum_cc_test(
    name = "procmon_test",
    srcs = ["procmon_test.cc"],
    deps = [
        ":procmon",
        ":procmon_proto",
        "@com_github_google_glog//:glog",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_binary(
    name = "procmon_main",
    srcs = ["procmon_main.cc"],
    copts = [
        "-lpthread",
        "-ldl",
        "-lrt",
    ],
    deps = [
        ":procmon",
        ":procmon_proto",
        ":procmon_service_impl",
        "@com_github_google_glog//:glog",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_google_grpc//:grpc++",
        #"//sandblaze/prebuilt/openssl",
        "//stratum/glue/status:status",
    ],
)

stratum_package(
    name = "procmon_main_pkg",
    bins = [
        ":procmon_main",
    ],
)
