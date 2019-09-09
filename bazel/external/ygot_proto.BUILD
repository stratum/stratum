#
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


package(
    default_visibility = [ "//visibility:public" ],
)

load(
    "@com_github_stratum_stratum//bazel/rules:proto_rule.bzl",
    "wrapped_proto_library",
)

PREFIX = "github.com/openconfig/ygot/proto/"

wrapped_proto_library(
    name = "ywrapper_proto",
    srcs = ["proto/ywrapper/ywrapper.proto"],
    new_proto_dir = PREFIX,
    proto_source_root = "proto/",
)

wrapped_proto_library(
    name = "yext_proto",
    srcs = ["proto/yext/yext.proto"],
    deps = ["@com_google_protobuf//:descriptor_proto"],
    new_proto_dir = PREFIX,
    proto_source_root = "proto/",
)

cc_proto_library(
    name = "ywrapper_cc_proto",
    deps = [":ywrapper_proto"]
)

cc_proto_library(
    name = "yext_cc_proto",
    deps = [":yext_proto"]
)