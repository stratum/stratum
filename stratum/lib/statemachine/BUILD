load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

licenses(["notice"])  # Apache v2

stratum_cc_library(
    name = "state_machine",
    hdrs = [
        "state_machine.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "example_state_machine",
    testonly = 1,
    srcs = [
        "example_state_machine.cc",
    ],
    hdrs = [
        "example_state_machine.h",
    ],
    deps = [
        ":state_machine",
        "//testing/base/public:gunit",
        "@com_google_absl//absl/container:flat_hash_map",
        "//stratum/glue/status:status",
    ],
)

stratum_cc_test(
    name = "state_machine_test",
    srcs = [
        "state_machine_test.cc",
    ],
    deps = [
        ":example_state_machine",
        ":state_machine",
        "//testing/base/public:gunit",
        "//testing/base/public:gunit_main",
        "//stratum/glue/status:status",
    ],
)
