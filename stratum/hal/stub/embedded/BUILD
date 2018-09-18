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
    "//stratum/portage:build_defs.bzl",
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
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/memory",
        "//absl/numeric:int128",
        "//absl/synchronization",
        "//protobuf",
        "//sandblaze/p4lang:code_proto",
        "//sandblaze/p4lang:p4runtime_v1_grpc_proto",
        "//sandblaze/prebuilt/grpc",
        "//sandblaze/prebuilt/openssl",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/glue/gnmi:gnmi_grpc_proto",
        "//stratum/glue/openconfig/proto:old_openconfig_proto",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:openconfig_converter",
        "//stratum/hal/lib/p4:p4_pipeline_config_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
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
