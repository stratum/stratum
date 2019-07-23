load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "HOST_ARCHES",
)

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "bf_switch",
    srcs = ["bf_switch.cc"],
    hdrs = ["bf_switch.h"],
    deps = [":bf_chassis_manager",
            "@com_google_googleapis//google/rpc:status_cc_proto",
            "//stratum/glue:integral_types",
            "//stratum/glue:logging",
            "//stratum/glue/status:status_macros",
            "//stratum/hal/lib/common:phal_interface",
            "//stratum/hal/lib/common:switch_interface",
            "//stratum/hal/lib/pi:pi_node_bf",
            "//stratum/lib:constants",
            "//stratum/lib:macros",
            "//stratum/glue/gtl:flat_hash_map",
            "//stratum/glue/gtl:map_util",
            "@local_barefoot_bin//:bf_headers",
    ],
)

stratum_cc_library(
    name = "bf_chassis_manager",
    srcs = ["bf_chassis_manager.cc"],
    hdrs = ["bf_chassis_manager.h"],
    deps = [":bf_pal_interface",
            "@com_github_google_glog//:glog",
            "@com_google_absl//absl/base:core_headers",
            "@com_google_absl//absl/memory",
            "@com_google_absl//absl/synchronization",
            "@com_google_absl//absl/types:optional",
            "@com_google_protobuf//:protobuf",
            "//stratum/glue:integral_types",
            "//stratum/glue:logging",
            "//stratum/glue/status",
            "//stratum/glue/status:statusor",
            "//stratum/hal/lib/common:common_cc_proto",
            "//stratum/hal/lib/common:constants",
            "//stratum/hal/lib/common:phal_interface",
            "//stratum/hal/lib/common:switch_interface",
            "//stratum/hal/lib/common:utils",
            "//stratum/hal/lib/common:writer_interface",
            "//stratum/lib:constants",
            "//stratum/lib:macros",
            "//stratum/lib:utils",
            "//stratum/public/lib:error",
            "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "bf_chassis_manager_test",
    srcs = ["bf_chassis_manager_test.cc"],
    deps = [":bf_pal_mock",
            ":bf_chassis_manager",
            ":test_main",
            "@com_google_googletest//:gtest",
            "//stratum/glue:integral_types",
            "//stratum/glue/status",
            "//stratum/glue/status:statusor",
            "//stratum/glue/status:status_test_util",
            "//stratum/hal/lib/common:common_cc_proto",
            "//stratum/hal/lib/common:constants",
            "//stratum/hal/lib/common:phal_mock",
            "//stratum/hal/lib/common:utils",
            "//stratum/hal/lib/common:writer_interface",
            "//stratum/lib/test_utils:matchers",
            "//stratum/lib:utils",
            "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "bf_pal_interface",
    hdrs = ["bf_pal_interface.h"],
    deps = ["@com_google_absl//absl/base:core_headers",
            "//stratum/glue:integral_types",
            "//stratum/glue/status",
            "//stratum/glue/status:statusor",
            "//stratum/hal/lib/common:common_cc_proto",
            "//stratum/lib/channel",
    ],
)

stratum_cc_library(
    name = "bf_pal_mock",
    testonly = 1,
    hdrs = ["bf_pal_mock.h"],
    deps = [":bf_pal_interface",
            "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bf_pal_wrapper",
    srcs = ["bf_pal_wrapper.cc"],
    hdrs = ["bf_pal_wrapper.h"],
    deps = [":bf_pal_interface",
            "@com_google_absl//absl/base:core_headers",
            "//stratum/glue:integral_types",
            "//stratum/glue:logging",
            "//stratum/glue/status",
            "//stratum/glue/status:statusor",
            "//stratum/hal/lib/common:common_cc_proto",
            "//stratum/lib/channel",
            "//stratum/lib:constants",
            "@local_barefoot_bin//:bf_headers",
            "@local_barefoot_bin//:bf_drivers",
    ],
)

stratum_cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)
