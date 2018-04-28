#
# Copyright 2018 Google LLC
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
    "//third_party/stratum/portage:build_defs.bzl",
    "STRATUM_INTERNAL",
    "sc_cc_lib",
    "sc_cc_test",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_cc_lib(
    name = "bits",
    srcs = [
        "bits.cc",
    ],
    hdrs = [
        "bits.h",
    ],
    deps = [
        "//third_party/absl/base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/numeric:int128",
        "//third_party/stratum/glue:logging",
    ],
)

sc_cc_test(
    name = "bits_test",
    size = "small",
    srcs = ["bits_test.cc"],
    deps = [
        ":bits",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/numeric:int128",
        "//third_party/stratum/glue:logging",
    ],
)

sc_cc_lib(
    name = "ipaddress",
    srcs = [
        "ipaddress.cc",
    ],
    hdrs = [
        "ipaddress.h",
    ],
    copts = [
        # TODO: hash_set -> unordered_set, then remove this.
        "-Wno-deprecated",
    ],
    deps = [
        ":bits",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/numeric:int128",
        "//third_party/absl/strings",
        "//third_party/stratum/glue:logging",
    ],
)

sc_cc_test(
    name = "ipaddress_test",
    size = "small",
    srcs = ["ipaddress_test.cc"],
    deps = [
        ":ipaddress",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/numeric:int128",
        "//third_party/absl/strings",
        "//third_party/stratum/glue:logging",
    ],
)

sc_cc_lib(
    name = "ports",
    srcs = [
        "ports.cc",
    ],
    hdrs = [
        "ports.h",
    ],
    deps = [
        "//third_party/stratum/glue:logging",
    ],
)
