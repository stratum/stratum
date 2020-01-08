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

load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

package(
    default_visibility = [ "//visibility:public" ],
)

proto_library(
    name = "types_proto",
    srcs = ["types/types.proto"],
    deps = [
        "@com_google_protobuf//:descriptor_proto",
        "@com_google_protobuf//:any_proto",
    ],
)

cc_proto_library(
    name = "types_cc_proto",
    deps = [":types_proto"]
)

proto_library(
    name = "common_proto",
    srcs = ["common/common.proto"],
    deps = [":types_proto"],
)

cc_proto_library(
    name = "common_cc_proto",
    deps = [":common_proto"],
)

proto_library(
    name = "diag_proto",
    srcs = ["diag/diag.proto"],
    deps = [":types_proto"],
)

cc_proto_library(
    name = "diag_cc_proto",
    deps = [":diag_proto"]
)

cc_grpc_library(
    name = "diag_cc_grpc",
    srcs = [":diag_proto"],
    deps = [":diag_cc_proto"],
    grpc_only = True,
)

proto_library(
    name = "system_proto",
    srcs = ["system/system.proto"],
    deps = [
      ":types_proto",
      ":common_proto"
    ],
)

cc_proto_library(
    name = "system_cc_proto",
    deps = [":system_proto"]
)

cc_grpc_library(
    name = "system_cc_grpc",
    srcs = [":system_proto"],
    deps = [":system_cc_proto"],
    grpc_only = True,
)

proto_library(
    name = "file_proto",
    srcs = ["file/file.proto"],
    deps = [
      ":types_proto",
      ":common_proto"
    ],
)

cc_proto_library(
    name = "file_cc_proto",
    deps = [":file_proto"],
)

cc_grpc_library(
    name = "file_cc_grpc",
    srcs = [":file_proto"],
    deps = [":file_cc_proto"],
    grpc_only = True,
)

proto_library(
    name = "cert_proto",
    srcs = ["cert/cert.proto"],
    deps = [
      ":types_proto",
      ":common_proto",
    ],
)

cc_proto_library(
    name = "cert_cc_proto",
    deps = [":cert_proto"],
)

cc_grpc_library(
    name = "cert_cc_grpc",
    srcs = [":cert_proto"],
    deps = [":cert_cc_proto"],
    grpc_only = True,
)
