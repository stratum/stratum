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
    name = "bits",
    srcs = [
        "bits.cc",
    ],
    hdrs = [
        "bits.h",
    ],
    deps = [
        #"//testing/production_stub/public:gunit_prod",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:int128",
        "//stratum/glue:logging",
        "//stratum/glue:integral_types",
    ],
)

stratum_cc_test(
    name = "bits_test",
    size = "small",
    srcs = ["bits_test.cc"],
    deps = [
        ":bits",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:int128",
        "//stratum/glue:logging",
        "//stratum/glue:integral_types",
    ],
)

stratum_cc_library(
    name = "ipaddress",
    srcs = [
        "ipaddress.cc",
    ],
    hdrs = [
        "ipaddress.h",
    ],
    deps = [
        ":bits",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/strings",
        "//stratum/glue:logging",
        "//stratum/glue:integral_types",
    ],
)

stratum_cc_test(
    name = "ipaddress_test",
    size = "small",
    srcs = ["ipaddress_test.cc"],
    deps = [
        ":ipaddress",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/strings",
        "//stratum/glue:logging",
        "//stratum/glue:integral_types",
    ],
)

stratum_cc_library(
    name = "ports",
    srcs = [
        "ports.cc",
    ],
    hdrs = [
        "ports.h",
    ],
    deps = [
        "//stratum/glue:logging",
    ],
)

cc_test(
  name = "absl_test",
  srcs = ["absl_test.cc"],
  deps = [
      "@com_google_absl//absl/strings",
      "@com_google_googletest//:gtest_main",
  ],
)
