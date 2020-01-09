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
    "stratum_cc_library",
    "stratum_cc_test",
    "STRATUM_INTERNAL",
)

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

cc_library(
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
