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
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/public/proto:p4_annotation_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
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
        "//base",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/public/lib:error",
        "//third_party/stratum/public/proto:p4_annotation_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
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
        "//third_party/absl/memory",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/lib:error",
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
        "//third_party/absl/strings",
        "//third_party/protobuf",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/proto:p4_annotation_proto",
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
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
    ],
)

sc_cc_lib(
    name = "p4_match_key",
    srcs = ["p4_match_key.cc"],
    hdrs = ["p4_match_key.h"],
    deps = [
        ":common_flow_entry_proto",
        ":p4_table_map_proto",
        "//third_party/absl/memory",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "p4_match_key_test",
    srcs = ["p4_match_key_test.cc"],
    deps = [
        ":p4_match_key",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "p4_runtime_interface",
    srcs = ["p4_runtime_interface.cc"],
    hdrs = ["p4_runtime_interface.h"],
    deps = [
        # The p4_device_config_proto rule contains symbols that are needed
        # by the embedded p4lang prebuilt.
        "//third_party/sandblaze/p4lang:p4_device_config_proto",
        "//third_party/sandblaze/p4lang:util",
        "//third_party/sandblaze/prebuilt/p4lang:vans-p4lang-pi",
    ],
)

sc_cc_lib(
    name = "p4_runtime_mock",
    testonly = 1,
    hdrs = ["p4_runtime_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_runtime_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_lib(
    name = "p4_runtime_real",
    srcs = ["p4_runtime_real.cc"],
    hdrs = ["p4_runtime_real.h"],
    deps = [
        ":p4_runtime_interface",
    ],
)

sc_proto_lib(
    name = "p4_control_proto",
    hdrs = ["p4_control.proto"],
    deps = [
        "//third_party/stratum/public/proto:p4_annotation_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
    ],
)

sc_proto_lib(
    name = "p4_pipeline_config_proto",
    hdrs = ["p4_pipeline_config.proto"],
    deps = [
        ":p4_control_proto",
        ":p4_table_map_proto",
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/public/proto:p4_annotation_proto",
    ],
)

sc_proto_lib(
    name = "p4_table_map_proto",
    hdrs = ["p4_table_map.proto"],
    deps = [
        ":common_flow_entry_proto",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/stratum/public/proto:p4_annotation_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
    ],
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
#        "//third_party/stratum/glue/status",
#        "//third_party/sandblaze/p4lang:p4runtime_proto",
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
        "//third_party/absl/memory",
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/lib:error",
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
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/net_util:ipaddress",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/hal/lib/common:common_proto",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/proto:hal_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
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
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
    ],
)

sc_cc_lib(
    name = "p4_write_request_differ",
    srcs = ["p4_write_request_differ.cc"],
    hdrs = ["p4_write_request_differ.h"],
    deps = [
        "//base",
        "//third_party/protobuf",
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "p4_write_request_differ_test",
    srcs = ["p4_write_request_differ_test.cc"],
    deps = [
        ":p4_write_request_differ",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/protobuf",
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":p4_runtime_interface",
        "//third_party/absl/strings",
    ],
)

sc_cc_test(
    name = "utils_test",
    srcs = [
        "utils_test.cc",
    ],
    deps = [
        ":p4_runtime_mock",
        ":testdata",
        ":utils",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/absl/strings",
        "//third_party/sandblaze/prebuilt/p4lang:vans-p4lang-pi",
        "//third_party/stratum/lib:utils",
    ],
)

sc_cc_lib(
    name = "testdata",
    arches = HOST_ARCHES,
    data = glob(["testdata/**"]),
)
