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
    "//build_util:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_cc_proto_library",
    "HOST_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_proto_library(
    name = "common_flow_entry_proto",
    hdrs = ["common_flow_entry.proto"],
    deps = [
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

stratum_cc_library(
    name = "p4_config_verifier",
    srcs = ["p4_config_verifier.cc"],
    hdrs = ["p4_config_verifier.h"],
    deps = [
        ":p4_pipeline_config_proto",
        ":p4_table_map_proto",
        ":p4_write_request_differ",
        "//base",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "//sandblaze/p4lang:p4info_proto",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "p4_config_verifier_test",
    srcs = ["p4_config_verifier_test.cc"],
    deps = [
        ":p4_config_verifier",
        ":p4_info_manager",
        ":p4_pipeline_config_proto",
        ":p4_table_map_proto",
        ":testdata",
        "//base",
        "//testing/base/public:gunit_main_no_google3",
        "@com_google_absl//absl/memory",
        "//sandblaze/p4lang:p4info_proto",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "//util/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "p4_info_manager",
    srcs = ["p4_info_manager.cc"],
    hdrs = ["p4_info_manager.h"],
    deps = [
        ":utils",
        "//base",
        "@com_google_absl//absl/strings",
        "//protobuf",
        "//sandblaze/p4lang:p4info_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_annotation_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "p4_info_manager_mock",
    testonly = 1,
    hdrs = ["p4_info_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_info_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_test(
    name = "p4_info_manager_test",
    srcs = ["p4_info_manager_test.cc"],
    deps = [
        ":p4_info_manager",
        ":testdata",
        "//base",
        "//testing/base/public:gunit_main_no_google3",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "//sandblaze/p4lang:p4info_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

stratum_cc_library(
    name = "p4_match_key",
    srcs = ["p4_match_key.cc"],
    hdrs = ["p4_match_key.h"],
    deps = [
        ":common_flow_entry_proto",
        ":p4_table_map_proto",
        "@com_google_absl//absl/memory",
        "//sandblaze/p4lang:p4info_proto",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_test(
    name = "p4_match_key_test",
    srcs = ["p4_match_key_test.cc"],
    deps = [
        ":p4_match_key",
        "//testing/base/public:gunit_main_no_google3",
        "//sandblaze/p4lang:p4info_proto",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "p4_runtime_interface",
    srcs = ["p4_runtime_interface.cc"],
    hdrs = ["p4_runtime_interface.h"],
    deps = [
        # The p4_device_config_proto rule contains symbols that are needed
        # by the embedded p4lang prebuilt.
        "//sandblaze/p4lang:p4_device_config_proto",
        "//sandblaze/p4lang:util",
        "//sandblaze/prebuilt/p4lang:vans-p4lang-pi",
    ],
)

stratum_cc_library(
    name = "p4_runtime_mock",
    testonly = 1,
    hdrs = ["p4_runtime_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_runtime_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_library(
    name = "p4_runtime_real",
    srcs = ["p4_runtime_real.cc"],
    hdrs = ["p4_runtime_real.h"],
    deps = [
        ":p4_runtime_interface",
    ],
)

stratum_cc_proto_library(
    name = "p4_control_proto",
    hdrs = ["p4_control.proto"],
    deps = [
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

stratum_cc_proto_library(
    name = "p4_pipeline_config_proto",
    hdrs = ["p4_pipeline_config.proto"],
    deps = [
        ":p4_control_proto",
        ":p4_table_map_proto",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/public/proto:p4_annotation_proto",
    ],
)

stratum_cc_proto_library(
    name = "p4_table_map_proto",
    hdrs = ["p4_table_map.proto"],
    deps = [
        ":common_flow_entry_proto",
        "//sandblaze/p4lang:p4info_proto",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

# TODO: Restore this rule if circular dependencies between
# p4_static_entry_mapper and p4_table_mapper can be resolved.
#stratum_cc_library(
#    name = "p4_static_entry_mapper",
#    srcs = ["p4_static_entry_mapper.cc"],
#    hdrs = ["p4_static_entry_mapper.h"],
#    deps = [
#        ":p4_table_mapper",
#        ":p4_pipeline_config_proto",
#        ":p4_write_request_differ",
#        "//base",
#        "//stratum/glue/status",
#        "//sandblaze/p4lang:p4runtime_proto",
#    ],
#)

stratum_cc_library(
    name = "p4_static_entry_mapper_mock",
    testonly = 1,
    hdrs = ["p4_static_entry_mapper_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_table_mapper",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_test(
    name = "p4_static_entry_mapper_test",
    srcs = ["p4_static_entry_mapper_test.cc"],
    deps = [
        ":p4_table_mapper",
        ":p4_table_mapper_mock",
        "//base",
        "//testing/base/public:gunit_main_no_google3",
        "@com_google_absl//absl/memory",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
    ],
)

# P4StaticEntryMapper and P4TableMapper are closely coupled and
# exist in the same library to avoid a circular dependency.
stratum_cc_library(
    name = "p4_table_mapper",
    srcs = [
        "p4_static_entry_mapper.cc",
        "p4_table_mapper.cc",
    ],
    hdrs = [
        "p4_static_entry_mapper.h",
        "p4_table_mapper.h",
    ],
    deps = [
        ":common_flow_entry_proto",
        ":p4_config_verifier",
        ":p4_info_manager",
        ":p4_match_key",
        ":p4_pipeline_config_proto",
        ":p4_table_map_proto",
        ":p4_write_request_differ",
        ":utils",
        "//base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "//sandblaze/p4lang:p4info_proto",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/glue:logging",
        "//stratum/glue/net_util:ipaddress",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:hal_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "p4_table_mapper_mock",
    testonly = 1,
    hdrs = ["p4_table_mapper_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_table_mapper",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_test(
    name = "p4_table_mapper_test",
    srcs = ["p4_table_mapper_test.cc"],
    deps = [
        ":p4_info_manager",
        ":p4_static_entry_mapper_mock",
        ":p4_table_mapper",
        ":testdata",
        "//base",
        "//testing/base/public:gunit_main_no_google3",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
    ],
)

stratum_cc_library(
    name = "p4_write_request_differ",
    srcs = ["p4_write_request_differ.cc"],
    hdrs = ["p4_write_request_differ.h"],
    deps = [
        "//base",
        "//protobuf",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_test(
    name = "p4_write_request_differ_test",
    srcs = ["p4_write_request_differ_test.cc"],
    deps = [
        ":p4_write_request_differ",
        "//testing/base/public:gunit_main_no_google3",
        "//protobuf",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":p4_runtime_interface",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_test(
    name = "utils_test",
    srcs = [
        "utils_test.cc",
    ],
    deps = [
        ":p4_runtime_mock",
        ":testdata",
        ":utils",
        "//testing/base/public:gunit_main_no_google3",
        "@com_google_absl//absl/strings",
        "//sandblaze/prebuilt/p4lang:vans-p4lang-pi",
        "//stratum/lib:utils",
    ],
)

stratum_cc_library(
    name = "testdata",
    arches = HOST_ARCHES,
    data = glob(["testdata/**"]),
)
