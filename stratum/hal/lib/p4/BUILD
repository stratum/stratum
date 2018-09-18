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
    "sc_cc_lib",
    "sc_cc_test",
    "sc_proto_lib",
    "HOST_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_proto_lib(
    name = "common_flow_entry_proto",
    hdrs = ["common_flow_entry.proto"],
    deps = [
        "//sandblaze/p4lang:p4runtime_v1_proto",
    ],
)

sc_proto_lib(
    name = "forwarding_pipeline_configs_proto",
    hdrs = ["forwarding_pipeline_configs.proto"],
    deps = [
        "//sandblaze/p4lang:p4runtime_v1_proto",
    ],
)

sc_cc_lib(
    name = "p4_action_mapper",
    srcs = ["p4_action_mapper.cc"],
    hdrs = ["p4_action_mapper.h"],
    deps = [
        ":p4_info_manager",
        ":p4_pipeline_config_proto",
        ":p4_table_map_proto",
        ":utils",
        "//platforms/networking/hercules/lib:macros",
        "//absl/container:flat_hash_map",
        "//absl/memory",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_test(
    name = "p4_action_mapper_test",
    srcs = ["p4_action_mapper_test.cc"],
    deps = [
        ":p4_action_mapper",
        ":p4_info_manager_mock",
        ":p4_pipeline_config_proto",
        ":p4_table_map_proto",
        ":testdata",
        "//platforms/networking/hercules/lib:utils",
        "//platforms/networking/hercules/lib/test_utils:matchers",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "p4_config_verifier",
    srcs = ["p4_config_verifier.cc"],
    hdrs = ["p4_config_verifier.h"],
    deps = [
        ":p4_pipeline_config_proto",
        ":p4_table_map_proto",
        ":p4_write_request_differ",
        ":utils",
        "//base",
        "//absl/memory",
        "//absl/strings",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//sandblaze/p4lang:p4runtime_v1_proto",
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

sc_cc_test(
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
        "//absl/memory",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "p4_info_manager",
    srcs = ["p4_info_manager.cc"],
    hdrs = ["p4_info_manager.h"],
    deps = [
        ":utils",
        "//base",
        "//absl/strings",
        "//protobuf",
        "//sandblaze/p4lang:p4info_v1_proto",
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

sc_cc_lib(
    name = "p4_info_manager_mock",
    testonly = 1,
    hdrs = ["p4_info_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_info_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "p4_info_manager_test",
    srcs = ["p4_info_manager_test.cc"],
    deps = [
        ":p4_info_manager",
        ":testdata",
        "//base",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

sc_cc_lib(
    name = "p4_match_key",
    srcs = ["p4_match_key.cc"],
    hdrs = ["p4_match_key.h"],
    deps = [
        ":common_flow_entry_proto",
        ":p4_table_map_proto",
        "//base",
        "//absl/memory",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "p4_match_key_test",
    srcs = ["p4_match_key_test.cc"],
    deps = [
        ":p4_match_key",
        "//testing/base/public:gunit_main_no_google3",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/public/lib:error",
    ],
)

sc_proto_lib(
    name = "p4_control_proto",
    hdrs = ["p4_control.proto"],
    deps = [
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

sc_proto_lib(
    name = "p4_pipeline_config_proto",
    hdrs = ["p4_pipeline_config.proto"],
    deps = [
        ":p4_control_proto",
        ":p4_table_map_proto",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/public/proto:p4_annotation_proto",
    ],
)

sc_proto_lib(
    name = "p4_table_map_proto",
    hdrs = ["p4_table_map.proto"],
    deps = [
        ":common_flow_entry_proto",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

filegroup(
    name = "proto_srcs",
    srcs = glob(["*.proto"]),
)

# TODO: Restore this rule if circular dependencies between
# p4_static_entry_mapper and p4_table_mapper can be resolved.
#sc_cc_lib(
#    name = "p4_static_entry_mapper",
#    srcs = ["p4_static_entry_mapper.cc"],
#    hdrs = ["p4_static_entry_mapper.h"],
#    deps = [
#        ":p4_table_mapper",
#        ":p4_pipeline_config_proto",
#        ":p4_write_request_differ",
#        "//base",
#        "//stratum/glue/status",
#        "//sandblaze/p4lang:p4runtime_v1_proto",
#    ],
#)

sc_cc_lib(
    name = "p4_static_entry_mapper_mock",
    testonly = 1,
    hdrs = ["p4_static_entry_mapper_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_table_mapper",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "p4_static_entry_mapper_test",
    srcs = ["p4_static_entry_mapper_test.cc"],
    deps = [
        ":p4_table_mapper",
        ":p4_table_mapper_mock",
        "//base",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

# P4StaticEntryMapper and P4TableMapper are closely coupled and
# exist in the same library to avoid a circular dependency.
sc_cc_lib(
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
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/memory",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue:logging",
        "//stratum/glue/net_util:ipaddress",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "p4_table_mapper_mock",
    testonly = 1,
    hdrs = ["p4_table_mapper_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_table_mapper",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "p4_table_mapper_test",
    srcs = ["p4_table_mapper_test.cc"],
    deps = [
        ":p4_info_manager",
        ":p4_static_entry_mapper_mock",
        ":p4_table_mapper",
        ":testdata",
        "//base",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/base:core_headers",
        "//absl/memory",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "p4_write_request_differ",
    srcs = ["p4_write_request_differ.cc"],
    hdrs = ["p4_write_request_differ.h"],
    deps = [
        "//base",
        "//protobuf",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "p4_write_request_differ_test",
    srcs = ["p4_write_request_differ_test.cc"],
    deps = [
        ":p4_write_request_differ",
        "//testing/base/public:gunit_main_no_google3",
        "//protobuf",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":p4_pipeline_config_proto",
        ":p4_table_map_proto",
        "//platforms/networking/hercules/lib:macros",
        "//platforms/networking/hercules/public/lib:error",
        "//absl/strings",
        "//absl/strings:str_format",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "utils_test",
    srcs = [
        "utils_test.cc",
    ],
    deps = [
        ":p4_pipeline_config_proto",
        ":p4_table_map_proto",
        ":testdata",
        ":utils",
        "//platforms/networking/hercules/lib/test_utils:matchers",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/strings",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//stratum/lib:utils",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "testdata",
    arches = HOST_ARCHES,
    data = glob(["testdata/**"]),
)
