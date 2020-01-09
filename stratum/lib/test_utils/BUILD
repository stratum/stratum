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

# This package contains testing libraries for Hercules.

load(
    "//bazel:rules.bzl",
    "stratum_cc_library",
    "stratum_cc_test",
    "STRATUM_INTERNAL",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "matchers",
    testonly = 1,
    hdrs = ["matchers.h"],
    deps = [
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "p4_proto_builders",
    testonly = 1,
    srcs = ["p4_proto_builders.cc"],
    hdrs = ["p4_proto_builders.h"],
    deps = [
        "//stratum/hal/lib/p4:p4_control_cc_proto",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
    ],
)

stratum_cc_test(
    name = "p4_proto_builders_test",
    srcs = ["p4_proto_builders_test.cc"],
    deps = [
        ":matchers",
        ":p4_proto_builders",
        "//stratum/lib:test_main",
        "//stratum/lib:utils",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/strings",
    ],
)
