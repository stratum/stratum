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

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_grpc_library", "py_proto_library")

package(
    default_visibility = [ "//visibility:public" ],
)

proto_library(
    name = "gnmi_ext_proto",
    srcs = ["gnmi_ext/gnmi_ext.proto"],
)

proto_library(
    name = "gnmi_proto",
    srcs = ["gnmi/gnmi.proto"],
    deps = [
        ":gnmi_ext_proto",
        "@com_google_protobuf//:descriptor_proto",
        "@com_google_protobuf//:any_proto",
    ],
)

cc_proto_library(
    name = "gnmi_ext_cc_proto",
    deps = [":gnmi_ext_proto"]
)

cc_proto_library(
    name = "gnmi_cc_proto",
    deps = [":gnmi_proto"],
)

cc_grpc_library(
    name = "gnmi_cc_grpc",
    srcs = [":gnmi_proto"],
    deps = [":gnmi_cc_proto"],
    grpc_only = True
)

py_proto_library(
    name = "gnmi_py_proto",
    deps = [":gnmi_proto"]
)

py_grpc_library(
    name = "gnmi_py_grpc",
    srcs = [":gnmi_proto"],
    deps = [":gnmi_py_proto"],
)
