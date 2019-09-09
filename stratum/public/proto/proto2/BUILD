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

# Google3 net/proto2 versions of platforms/networking/hercules/public/proto
# protobufs.

# Including both third-party proto3 libraries & google3 proto libraries
# leads to compile-time name collisions. These build rules generate google3
# proto2 libraries that will compile alongside other google3 proto libraries.
load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

licenses(["notice"])  # Apache v2

genrule(
    name = "proto2_srcs",
    srcs = ["//stratum/public/proto:proto_srcs"],
    outs = [
        "p4_table_defs.proto",
        "p4_annotation.proto",
    ],
    # Replace:
    #   import "platforms/networking/hercules/.../pkg_proto"
    # with...
    #   import "platforms/networking/hercules/.../proto2/pkg_proto"
    cmd = ("cp $(SRCS) $(@D);" +
           "sed -i 's|\\(import.*platforms/networking/hercules/.*\\)/|\\1/proto2/|' $(OUTS);"),
    visibility = ["//visibility:private"],
)

proto_library(
    name = "p4_table_defs_proto",
    srcs = ["p4_table_defs.proto"],
    #cc_api_version = 2, FIXME(boc) google only
)

proto_library(
    name = "p4_annotation_proto",
    srcs = ["p4_annotation.proto"],
    #cc_api_version = 2, FIXME(boc) google only
    deps = [":p4_table_defs_proto"],
)
