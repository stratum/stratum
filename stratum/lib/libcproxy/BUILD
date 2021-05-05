#
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
)

licenses(["notice"])  # Apache v2

package(
    # default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "passthrough_proxy",
    srcs = [
        "passthrough_proxy.cc",
    ],
    hdrs = [
        "passthrough_proxy.h",
    ],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
    ],
)

# TODO(max)
# stratum_cc_test(
#     name = "libcproxy_test",
#     srcs = [
#         "libcproxy_test.cc",
#     ],
#     deps = [
#         "@com_google_googletest//:gtest",
#         "//stratum/glue/status:status_test_util",
#         "//stratum/lib/test_utils:matchers",
#     ],
# )

stratum_cc_library(
    name = "libcwrapper",
    srcs = [
        "libcwrapper.cc",
    ],
    hdrs = [
        "libcwrapper.h",
    ],
    deps = [
        ":passthrough_proxy",
    ],
)

# TODO(max)
# stratum_cc_test(
#     name = "libcwrapper_test",
#     srcs = [
#         "libcwrapper_test.cc",
#     ],
#     deps = [
#         "@com_google_googletest//:gtest",
#         "//stratum/glue/status:status_test_util",
#         "//stratum/lib/test_utils:matchers",
#     ],
# )
