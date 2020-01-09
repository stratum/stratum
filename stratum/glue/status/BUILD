#
# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
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
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "status",
    srcs = [
        "canonical_errors.cc",
        "posix_error_space.cc",
        "status.cc",
    ],
    hdrs = [
        "canonical_errors.h",
        "posix_error_space.h",
        "status.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:source_location",
    ],
)

stratum_cc_library(
    name = "statusor",
    srcs = [
        "statusor.cc",
    ],
    hdrs = [
        "statusor.h",
    ],
    deps = [
        ":status",
        "//stratum/glue:logging",
    ],
)

stratum_cc_library(
    name = "status_macros",
    srcs = [
        "status_macros.cc",
    ],
    hdrs = [
        "status_macros.h",
    ],
    deps = [
        ":status",
        ":statusor",
        "@com_google_absl//absl/strings",
        "//stratum/glue:logging",
    ],
)

cc_library(
    name = "status_test_util",
    testonly = 1,
    srcs = [
        "status_test_util.cc",
    ],
    hdrs = [
        "status_test_util.h",
    ],
    deps = [
        ":status",
        ":statusor",
        "@com_google_googletest//:gtest",
        "//stratum/public/proto:error_cc_proto",
    ],
)

stratum_cc_test(
    name = "status_test",
    size = "small",
    srcs = [
        "posix_error_space_test.cc",
        "status_test.cc",
        "status_test_util_test.cc",
        "statusor_test.cc",
    ],
    deps = [
        ":status",
        ":status_test_util",
        ":statusor",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)
