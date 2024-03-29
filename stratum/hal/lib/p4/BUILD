# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

proto_library(
    name = "common_flow_entry_proto",
    srcs = ["common_flow_entry.proto"],
    deps = [
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_proto",
    ],
)

cc_proto_library(
    name = "common_flow_entry_cc_proto",
    deps = [":common_flow_entry_proto"],
)

proto_library(
    name = "forwarding_pipeline_configs_proto",
    srcs = ["forwarding_pipeline_configs.proto"],
    deps = [
        "@com_github_p4lang_p4runtime//:p4runtime_proto",
    ],
)

cc_proto_library(
    name = "forwarding_pipeline_configs_cc_proto",
    deps = [":forwarding_pipeline_configs_proto"],
)

stratum_cc_library(
    name = "p4_action_mapper",
    srcs = ["p4_action_mapper.cc"],
    hdrs = ["p4_action_mapper.h"],
    deps = [
        ":p4_info_manager",
        ":p4_pipeline_config_cc_proto",
        ":p4_table_map_cc_proto",
        ":utils",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
        "//stratum/lib:macros",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
    ],
)

stratum_cc_test(
    name = "p4_action_mapper_test",
    srcs = ["p4_action_mapper_test.cc"],
    deps = [
        ":p4_action_mapper",
        ":p4_info_manager_mock",
        ":p4_pipeline_config_cc_proto",
        ":p4_table_map_cc_proto",
        ":testdata",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "p4_config_verifier",
    srcs = ["p4_config_verifier.cc"],
    hdrs = ["p4_config_verifier.h"],
    deps = [
        ":p4_pipeline_config_cc_proto",
        ":p4_table_map_cc_proto",
        ":p4_write_request_differ",
        ":utils",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_test(
    name = "p4_config_verifier_test",
    srcs = ["p4_config_verifier_test.cc"],
    deps = [
        ":p4_config_verifier",
        ":p4_info_manager",
        ":p4_pipeline_config_cc_proto",
        ":p4_table_map_cc_proto",
        ":testdata",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "p4_info_manager",
    srcs = ["p4_info_manager.cc"],
    hdrs = ["p4_info_manager.h"],
    deps = [
        ":utils",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
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
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "p4_match_key",
    srcs = ["p4_match_key.cc"],
    hdrs = ["p4_match_key.h"],
    deps = [
        ":common_flow_entry_cc_proto",
        ":p4_table_map_cc_proto",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/memory",
    ],
)

stratum_cc_test(
    name = "p4_match_key_test",
    srcs = ["p4_match_key_test.cc"],
    copts = ["-funsigned-char"],
    deps = [
        ":p4_match_key",
        "//stratum/glue/status:status_test_util",
        "//stratum/public/lib:error",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "p4_control_proto",
    srcs = ["p4_control.proto"],
    deps = [
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

cc_proto_library(
    name = "p4_control_cc_proto",
    deps = [":p4_control_proto"],
)

proto_library(
    name = "p4_pipeline_config_proto",
    srcs = ["p4_pipeline_config.proto"],
    deps = [
        ":p4_control_proto",
        ":p4_table_map_proto",
        "//stratum/public/proto:p4_annotation_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_proto",
    ],
)

cc_proto_library(
    name = "p4_pipeline_config_cc_proto",
    deps = [":p4_pipeline_config_proto"],
)

proto_library(
    name = "p4_table_map_proto",
    srcs = ["p4_table_map.proto"],
    deps = [
        ":common_flow_entry_proto",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "@com_github_p4lang_p4runtime//:p4info_proto",
    ],
)

cc_proto_library(
    name = "p4_table_map_cc_proto",
    deps = [":p4_table_map_proto"],
)

filegroup(
    name = "proto_srcs",
    srcs = glob(["*.proto"]),
)

# TODO(unknown): Restore this rule if circular dependencies between
# p4_static_entry_mapper and p4_table_mapper can be resolved.
#stratum_cc_library(
#    name = "p4_static_entry_mapper",
#    srcs = ["p4_static_entry_mapper.cc"],
#    hdrs = ["p4_static_entry_mapper.h"],
#    deps = [
#        ":p4_table_mapper",
#        ":p4_pipeline_config_cc_proto",
#        ":p4_write_request_differ",
#        "@com_github_google_glog//:glog",
#        "//stratum/glue/status",
#        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
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
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",  #FIXME actually p4runtime_cc_proto
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
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
        ":common_flow_entry_cc_proto",
        ":p4_config_verifier",
        ":p4_info_manager",
        ":p4_match_key",
        ":p4_pipeline_config_cc_proto",
        ":p4_table_map_cc_proto",
        ":p4_write_request_differ",
        ":utils",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/net_util:ipaddress",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",  #FIXME actually p4runtime_cc_proto
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
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
        "//stratum/glue:logging",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "p4_write_request_differ",
    srcs = ["p4_write_request_differ.cc"],
    hdrs = ["p4_write_request_differ.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",  #FIXME actually p4runtime_cc_proto
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_test(
    name = "p4_write_request_differ_test",
    srcs = ["p4_write_request_differ_test.cc"],
    deps = [
        ":p4_write_request_differ",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",  #FIXME actually p4runtime_cc_proto
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":p4_pipeline_config_cc_proto",
        ":p4_table_map_cc_proto",
        "//stratum/glue:integral_types",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_test(
    name = "utils_test",
    srcs = [
        "utils_test.cc",
    ],
    deps = [
        ":p4_pipeline_config_cc_proto",
        ":p4_table_map_cc_proto",
        ":testdata",
        ":utils",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "testdata",
    arches = HOST_ARCHES,
    data = glob(["testdata/**"]),
)
