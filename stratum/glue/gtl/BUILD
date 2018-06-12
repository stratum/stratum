#
# Copyright 2018 Open Networking Foundation
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

exports_files(["LICENSE"])

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

cc_library(
    name = "stl_util",
    hdrs = ["stl_util.h"],
)

cc_library(
    name = "map_util",
    hdrs = ["map_util.h"],
)

cc_test(
    name = "map_util_test",
    srcs = ["map_util_test.cc"],
    deps = [
        ":map_util",
        "@com_google_googletest//:gtest_main",
        "//stratum/glue:integral_types",
    ],
)

cc_library(
    name = "source_location",
    hdrs = ["source_location.h"],
)
