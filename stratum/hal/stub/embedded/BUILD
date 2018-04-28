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
    "sc_cc_bin",
    "sc_package",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_cc_bin(
    name = "stratum_stub",
    srcs = ["main.cc"],
    copts = [
        "-lpthread",
        "-ldl",
        "-lrt",
    ],
    deps = [
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/numeric:int128",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/sandblaze/gnmi:gnmi_grpc_proto",
        "//third_party/sandblaze/p4lang:code_proto",
        "//third_party/sandblaze/p4lang:p4runtime_grpc_proto",
        "//third_party/sandblaze/p4lang:util",
        "//third_party/sandblaze/prebuilt/grpc",
        "//third_party/sandblaze/prebuilt/openssl",
        "//third_party/stratum/glue:init_google",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/hal/lib/common:conversion",
        "//third_party/stratum/hal/lib/p4:p4_pipeline_config_proto",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/proto:hal_grpc_proto",
        "//third_party/stratum/public/proto:openconfig_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_package(
    name = "stratum_stub_pkg",
    bins = [
        ":stratum_stub",
    ],
)
