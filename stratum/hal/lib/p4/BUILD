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
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_cc_proto_library",
    "HOST_ARCHES",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_proto_library(
    name = "common_flow_entry_proto",
    hdrs = ["common_flow_entry.proto"],
    deps = [
        "@com_github_p4lang_PI//:p4runtime_proto",
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
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_PI//:p4info_proto",
        "@com_github_p4lang_PI//:p4runtime_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//stratum/glue/gtl:map_util",
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
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_PI//:p4info_proto",
        "@com_github_p4lang_PI//:p4runtime_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "p4_info_manager",
    srcs = ["p4_info_manager.cc"],
    hdrs = ["p4_info_manager.h"],
    deps = [
        ":utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_PI//:p4info_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_annotation_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "p4_info_manager_mock",
    testonly = 1,
    hdrs = ["p4_info_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_info_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "p4_info_manager_test",
    srcs = ["p4_info_manager_test.cc"],
    deps = [
        ":p4_info_manager",
        ":testdata",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_PI//:p4info_proto",
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
        "@com_github_p4lang_PI//:p4info_proto",
        "@com_github_p4lang_PI//:p4runtime_proto",
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
        "@com_google_googletest//:gtest_main",
        "@com_github_p4lang_PI//:p4info_proto",
        "@com_github_p4lang_PI//:p4runtime_proto",
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
        "@com_github_p4lang_PI//:p4_device_config_proto",
        "@com_github_p4lang_PI//:util",
        "@com_github_p4lang_PI//:p4runtime_proto",
    ],
)

stratum_cc_library(
    name = "p4_runtime_mock",
    testonly = 1,
    hdrs = ["p4_runtime_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_runtime_interface",
        "@com_google_googletest//:gtest",
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
        "@com_github_p4lang_PI//:p4runtime_proto",
        "//stratum/public/proto:p4_annotation_proto",
    ],
)

stratum_cc_proto_library(
    name = "p4_table_map_proto",
    hdrs = ["p4_table_map.proto"],
    deps = [
        ":common_flow_entry_proto",
        "@com_github_p4lang_PI//:p4info_proto",
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
#        "@com_github_google_glog//:glog",
#        "//stratum/glue/status",
#        "@com_github_p4lang_PI//:p4runtime_proto",
#    ],
#)

stratum_cc_library(
    name = "p4_static_entry_mapper_mock",
    testonly = 1,
    hdrs = ["p4_static_entry_mapper_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_table_mapper",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "p4_static_entry_mapper_test",
    srcs = ["p4_static_entry_mapper_test.cc"],
    deps = [
        ":p4_table_mapper",
        ":p4_table_mapper_mock",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_PI//:p4runtime_proto",
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
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_PI//:p4info_proto",
        "@com_github_p4lang_PI//:p4runtime_proto",
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
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "p4_table_mapper_mock",
    testonly = 1,
    hdrs = ["p4_table_mapper_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":p4_table_mapper",
        "@com_google_googletest//:gtest",
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
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
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
        "@com_github_google_glog//:glog",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_PI//:p4runtime_proto",
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
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_PI//:p4runtime_proto",
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
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_PI//:p4runtime_proto",
        "//stratum/lib:utils",
    ],
)

stratum_cc_library(
    name = "testdata",
    arches = HOST_ARCHES,
    data = glob(["testdata/**"]),
)
