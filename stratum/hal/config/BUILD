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

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_package",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

licenses(["notice"])  # Apache v2

genrule(
    name = "phal_config_generic_trident2",
    outs = ["legacy_phal_init_config_generic_trident2_hercules.pb.txt"],
    cmd = (
        "$(location //stratum/hal/config/tools:google_config_generator) " +
        "--platform=generic_trident2 --config_type=phal --output=$@"
    ),
    tools = ["//stratum/hal/config/tools:google_config_generator"],
)

genrule(
    name = "phal_config_generic_trident2",
    outs = ["legacy_phal_init_config_generic_trident2_hercules.pb.txt"],
    cmd = (
        "$(location //stratum/hal/config/tools:google_config_generator) " +
        "--platform=generic_trident2 --config_type=phal --output=$@"
    ),
    tools = ["//stratum/hal/config/tools:google_config_generator"],
)

stratum_package(
    name = "hercules_config_files",
    data = glob(["*.txt"]) + glob(["*.bin"]) + [
        "legacy_phal_init_config_generic_trident2_hercules.pb.txt",
        "legacy_phal_init_config_generic_trident2_hercules.pb.txt",
    ],
)

filegroup(
    name = "bcm_sim_config_files",
    srcs = [
        "base_bcm_chassis_map_generic_tomahawk_hercules.pb.txt",
        "base_bcm_chassis_map_generic_trident2_hercules.pb.txt",
        "base_bcm_chassis_map_generic_trident2_hercules.pb.txt",
        "bcm_hardware_specs.pb.txt",
        "generic_trident2_bcm_serdes_db_hercules.pb.bin",
        "generic_trident2_bcm_serdes_db_hercules.pb.bin",
    ],
)
