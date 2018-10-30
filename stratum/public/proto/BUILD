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
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_proto_library",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_proto_library(
    name = "error_proto",
    hdrs = [
        "error.proto",
    ],
)

stratum_cc_proto_library(
    name = "p4_annotation_proto",
    hdrs = [
        "p4_annotation.proto",
    ],
    deps = [
        ":p4_table_defs_proto",
    ],
)

stratum_cc_proto_library(
    name = "p4_table_defs_proto",
    hdrs = [
        "p4_table_defs.proto",
    ],
)

proto_library(
    name = "tv_proto",
    srcs = ["tv.proto"],
    cc_api_version = 2,
    deps = [
        "//openconfig/gnmi/proto/gnmi:gnmi_proto",
        "//p4lang_PI/proto/p4/v1:p4runtime_proto",
    ],
)

filegroup(
    name = "proto_srcs",
    srcs = glob(["*.proto"]),
)
