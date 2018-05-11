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
    "sc_proto_lib",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_proto_lib(
    name = "openconfig_proto",
    hdrs = [
        "openconfig.proto",
    ],
    deps = [
        "//third_party/stratum/public/proto:sc_yang_proto",
        "//third_party/stratum/public/proto/openconfig/enums:enums_proto",
        "//third_party/stratum/public/proto/openconfig/hercules:hercules_proto",
        "//third_party/stratum/public/proto/openconfig/hercules_platform_asic_bcm:platform_asic_bcm_proto",
        "//third_party/stratum/public/proto/openconfig/openconfig_interfaces:openconfig_interfaces_proto",
        "//third_party/stratum/public/proto/openconfig/openconfig_lacp:openconfig_lacp_proto",
        "//third_party/stratum/public/proto/openconfig/openconfig_platform:openconfig_platform_proto",
        "//third_party/stratum/public/proto/openconfig/openconfig_qos:openconfig_qos_proto",
    ],
)
