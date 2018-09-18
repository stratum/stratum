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

# This package contains testing libraries for Hercules.

load(
    "//stratum/portage:build_defs.bzl",
    "sc_cc_lib",
    "sc_cc_test",
    "STRATUM_INTERNAL",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_cc_lib(
    name = "matchers",
    testonly = 1,
    hdrs = ["matchers.h"],
    deps = [
        "//testing/base/public:gunit_no_google3",
        "//testing/base/public:status_matchers_no_google3",
        "//protobuf",
    ],
)

sc_cc_lib(
    name = "p4_proto_builders",
    testonly = 1,
    srcs = ["p4_proto_builders.cc"],
    hdrs = ["p4_proto_builders.h"],
    deps = [
        "//platforms/networking/hercules/hal/lib/p4:p4_control_proto",
        "//platforms/networking/hercules/public/proto:p4_annotation_proto",
        "//platforms/networking/hercules/public/proto:p4_table_defs_proto",
        "//absl/strings",
        "//sandblaze/p4lang:p4info_v1_proto",
    ],
)

sc_cc_test(
    name = "p4_proto_builders_test",
    srcs = ["p4_proto_builders_test.cc"],
    deps = [
        ":matchers",
        ":p4_proto_builders",
        "//platforms/networking/hercules/lib:test_main",
        "//platforms/networking/hercules/lib:utils",
        "//testing/base/public:gunit_no_google3",
        "//absl/strings",
    ],
)
