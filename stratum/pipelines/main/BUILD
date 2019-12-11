# Copyright 2019-present Open Networking Foundation
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

licenses(["notice"])  # Apache v2

# Build the main P4 pipeline for Broadcom switches.

load("//stratum/p4c_backends/common:build_defs.bzl", "p4_fpm_compile")

load("//bazel/rules:p4c_build_defs.bzl", "p4_bmv2_compile")

filegroup(
    name = "annotation_map_files",
    srcs = [
        "field_map.pb.txt",
        "table_map.pb.txt",
    ],
)

p4_fpm_compile(
    name = "main_fpm",
    src = "main.p4",
    hdrs = [],
    annotation_maps = [":annotation_map_files"],
    out_p4_info = "fpm/main.p4info",
    out_p4_pipeline_binary = "fpm/main.pb.bin",
    out_p4_pipeline_text = "fpm/main.pb.txt",
)

p4_bmv2_compile(
    name = "main_bmv2",
    src = "main.p4",
    hdrs = [],
    out_p4_info = "bmv2/main.p4info",
    out_p4_pipeline_json = "bmv2/main.json",
)
