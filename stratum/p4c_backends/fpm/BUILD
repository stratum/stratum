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

# This file builds the switch-specific p4c_backends components.
load("//bazel:defs.bzl", "STRATUM_INTERNAL")
load("//bazel/rules:library_rule.bzl", "stratum_cc_library")
load(
    "//stratum/p4c_backends/test:build_defs.bzl",
    "p4c_save_ir",
)

package(
    # default_copts = [
    #     "-DDISABLE_GOOGLE_GLOBAL_USING_DECLARATIONS",
    # ],
    # default_hdrs_check = "warn",
    default_visibility = STRATUM_INTERNAL,
)


"""google only
# This rule makes a copy of production P4 role specs for generating local test
# input IR files.
genrule(
    name = "p4_spec_files",
    srcs = [
        "//platforms/networking/orion/p4/spec:p4roles",
    ],
    outs = [
        "testdata/b4.p4",
        "testdata/fbr_s2.p4",
        "testdata/fbr_s3.p4",
        "testdata/middleblock.p4",
        "testdata/spine.p4",
        "testdata/tor.p4",
    ],
    cmd = "cp $(SRCS) $(@D)/testdata",
)
"""

proto_library(
    name = "p4_annotation_map_proto",
    srcs = ["annotation_map.proto"],
    deps = [
        "//stratum/hal/lib/p4:common_flow_entry_proto",
        "//stratum/hal/lib/p4:p4_table_map_proto",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

cc_proto_library(
    name = "p4_annotation_map_cc_proto",
    deps = [":p4_annotation_map_proto"],
)

proto_library(
    name = "p4_model_names_proto",
    srcs = ["p4_model_names.proto"],
)

cc_proto_library(
    name = "p4_model_names_cc_proto",
    deps = [":p4_model_names_proto"],
)

proto_library(
    name = "p4_parser_map_proto",
    srcs = ["parser_map.proto"],
    deps = [
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

cc_proto_library(
    name = "p4_parser_map_cc_proto",
    deps = [":p4_parser_map_proto"],
)

proto_library(
    name = "sliced_field_map_proto",
    srcs = ["sliced_field_map.proto"],
    deps = [
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

cc_proto_library(
    name = "sliced_field_map_cc_proto",
    deps = [":sliced_field_map_proto"],
)

cc_library(
    name = "action_decoder",
    srcs = ["action_decoder.cc"],
    hdrs = ["action_decoder.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":expression_inspector",
        ":field_name_inspector",
        ":method_call_decoder",
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "//stratum/lib:utils",
    ],
)

cc_test(
    name = "action_decoder_test",
    srcs = ["action_decoder_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/action_assignments.ir.json",
        "testdata/action_misc.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":action_decoder",
        ":table_map_generator_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "annotation_mapper",
    srcs = ["annotation_mapper.cc"],
    hdrs = ["annotation_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4_annotation_map_cc_proto",
        "//stratum/glue:logging",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "//stratum/hal/lib/p4:common_flow_entry_cc_proto",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

# Annotation map files for unit tests.
cc_library(
    name = "annotation_testdata",
    data = [
        "testdata/annotation_string_map.pb.txt",
        "testdata/object_name_map.pb.txt",
    ],
)

cc_test(
    name = "annotation_mapper_test",
    srcs = ["annotation_mapper_test.cc"],
    deps = [
        ":annotation_mapper",
        ":annotation_testdata",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "//stratum/hal/lib/p4:common_flow_entry_cc_proto",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:macros",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        # "//util/task:status",
        "//stratum/glue/status:status",
    ],
)

cc_library(
    name = "condition_inspector",
    srcs = ["condition_inspector.cc"],
    hdrs = ["condition_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//stratum/glue:logging",
        "@com_google_absl//absl/strings:str_format",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
    ],
)

cc_test(
    name = "condition_inspector_test",
    srcs = ["condition_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":condition_inspector",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_library(
    name = "control_inspector",
    srcs = ["control_inspector.cc"],
    hdrs = ["control_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":condition_inspector",
        ":internal_action",
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        ":switch_case_decoder",
        ":table_map_generator",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_control_cc_proto",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
    ],
)

cc_test(
    name = "control_inspector_test",
    srcs = ["control_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/control_apply_hit_miss_test.ir.json",
        "testdata/control_hit_meter.ir.json",
        "testdata/control_if_test.ir.json",
        "testdata/control_misc_test.ir.json",
        "testdata/control_misc_test2.ir.json",
        "testdata/no_table_tmp.ir.json",
        "testdata/pipeline_opt_block.ir.json",
        "testdata/switch_case.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":control_inspector",
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        ":pipeline_optimizer",
        ":switch_case_decoder_mock",
        ":table_map_generator",
        ":table_map_generator_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/p4c_backends/test:test_target_info",
        "//stratum/public/proto:p4_annotation_cc_proto",
    ],
)

cc_library(
    name = "expression_inspector",
    srcs = ["expression_inspector.cc"],
    hdrs = ["expression_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":field_name_inspector",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_test(
    name = "expression_inspector_test",
    srcs = ["expression_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/action_assignments.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":expression_inspector",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_library(
    name = "field_cross_reference",
    srcs = ["field_cross_reference.cc"],
    hdrs = ["field_cross_reference.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

cc_test(
    name = "field_cross_reference_test",
    srcs = ["field_cross_reference_test.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":field_cross_reference",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_library(
    name = "field_decoder",
    srcs = ["field_decoder.cc"],
    hdrs = ["field_decoder.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":field_name_inspector",
        ":header_path_inspector",
        ":p4_model_names_cc_proto",
        ":p4_parser_map_cc_proto",
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
    ],
)

cc_test(
    name = "field_decoder_test",
    srcs = ["field_decoder_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/header_decode_advanced.ir.json",
        "testdata/header_decode_basic.ir.json",
        "testdata/header_decode_controller.ir.json",
        "//stratum/p4c_backends/test:testdata/simple_vlan_stack_16.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":field_decoder",
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "field_name_inspector",
    srcs = ["field_name_inspector.cc"],
    hdrs = ["field_name_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "field_name_inspector_test",
    srcs = ["field_name_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        ":testdata/field_inspect_test.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":field_name_inspector",
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "header_path_inspector",
    srcs = ["header_path_inspector.cc"],
    hdrs = ["header_path_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
    ],
)

cc_test(
    name = "header_path_inspector_test",
    srcs = ["header_path_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/header_decode_advanced.ir.json",
        "testdata/header_decode_basic.ir.json",
        "testdata/header_decode_stack.ir.json",
        "testdata/nested_metadata_test.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":header_path_inspector",
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "header_valid_inspector",
    srcs = ["header_valid_inspector.cc"],
    hdrs = ["header_valid_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":field_name_inspector",
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
    ],
)

cc_test(
    name = "header_valid_inspector_test",
    srcs = ["header_valid_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/header_valid_tests.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":header_valid_inspector",
        ":p4c_switch_utils",
        ":table_map_generator_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "hidden_static_mapper",
    srcs = ["hidden_static_mapper.cc"],
    hdrs = ["hidden_static_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":hidden_table_mapper",
        ":internal_action",
        ":p4c_switch_utils",
        ":tunnel_optimizer_interface",
        "//stratum/glue:logging",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_match_key",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
    ],
)

cc_test(
    name = "hidden_static_mapper_test",
    srcs = ["hidden_static_mapper_test.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":hidden_static_mapper",
        ":hidden_table_mapper",
        ":p4c_switch_utils",
        ":table_map_generator",
        ":tunnel_optimizer_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

cc_library(
    name = "hidden_table_mapper",
    srcs = ["hidden_table_mapper.cc"],
    hdrs = ["hidden_table_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
    ],
)

cc_test(
    name = "hidden_table_mapper_test",
    srcs = ["hidden_table_mapper_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/hidden_table1.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":hidden_table_mapper",
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

cc_library(
    name = "hit_assign_mapper",
    srcs = ["hit_assign_mapper.cc"],
    hdrs = ["hit_assign_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//stratum/glue:logging",
        "//stratum/lib:macros",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
    ],
)

cc_test(
    name = "hit_assign_mapper_test",
    srcs = ["hit_assign_mapper_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/control_apply_hit_miss_test.ir.json",
        "testdata/control_if_test.ir.json",
        "testdata/control_misc_test.ir.json",
        "testdata/hidden_table1.ir.json",
        "testdata/hit_assign.ir.json",
        "testdata/if_color_test.ir.json",
        "testdata/no_table_tmp.ir.json",
        "testdata/switch_case.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":hit_assign_mapper",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/p4c_backends/test:test_inspectors",
    ],
)

cc_library(
    name = "internal_action",
    srcs = ["internal_action.cc"],
    hdrs = ["internal_action.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4c_switch_utils",
        ":table_map_generator",
        ":tunnel_optimizer_interface",
        "//stratum/glue:logging",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

cc_test(
    name = "internal_action_test",
    srcs = ["internal_action_test.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":internal_action",
        ":p4c_switch_utils",
        ":table_map_generator",
        ":table_map_generator_mock",
        ":tunnel_optimizer_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_library(
    name = "meta_key_mapper",
    srcs = ["meta_key_mapper.cc"],
    hdrs = ["meta_key_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_test(
    name = "meta_key_mapper_test",
    srcs = ["meta_key_mapper_test.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":meta_key_mapper",
        ":table_map_generator_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "meter_color_mapper",
    srcs = ["meter_color_mapper.cc"],
    hdrs = ["meter_color_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":field_name_inspector",
        ":method_call_decoder",
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
    ],
)

cc_test(
    name = "meter_color_mapper_test",
    srcs = ["meter_color_mapper_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/control_apply_hit_miss_test.ir.json",
        "testdata/control_if_test.ir.json",
        "testdata/control_misc_test.ir.json",
        "testdata/if_color_test.ir.json",
        "testdata/if_color_test_errors.ir.json",
        "testdata/meter_color_errors1.ir.json",
        "testdata/meter_color_if_else.ir.json",
        "testdata/meter_color_nested_ifs.ir.json",
        "testdata/meter_colors.ir.json",
        "testdata/switch_case.ir.json",
        "testdata/switch_case_errors.ir.json",
        "testdata/switch_case_errors2.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":meter_color_mapper",
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        ":table_map_generator",
        ":table_map_generator_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
        "//stratum/lib:utils",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "method_call_decoder",
    srcs = ["method_call_decoder.cc"],
    hdrs = ["method_call_decoder.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":field_name_inspector",
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_test(
    name = "method_call_decoder_test",
    srcs = ["method_call_decoder_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/method_calls.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":method_call_decoder",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_library(
    name = "midend",
    srcs = ["midend.cc"],
    hdrs = ["midend.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/p4c_backends/common:midend_interface",
    ],
)

cc_test(
    name = "midend_test",
    srcs = ["midend_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/no_table_tmp.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":midend",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/p4c_backends/common:program_inspector",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "parser_decoder",
    srcs = ["parser_decoder.cc"],
    hdrs = ["parser_decoder.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":field_name_inspector",
        ":p4_model_names_cc_proto",
        ":p4_parser_map_cc_proto",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
    ],
)

cc_test(
    name = "parser_decoder_test",
    srcs = ["parser_decoder_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/parse_basic.ir.json",
        "testdata/parse_complex.ir.json",
        "testdata/parse_value_set.ir.json",
        "testdata/parse_annotated_state.ir.json",
        "//stratum/p4c_backends/test:testdata/simple_vlan_stack_16.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":parser_decoder",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "parser_field_mapper",
    srcs = ["parser_field_mapper.cc"],
    hdrs = ["parser_field_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":field_decoder",
        ":p4_parser_map_cc_proto",
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "//stratum/glue/gtl:map_util",
    ],
)

cc_test(
    name = "parser_field_mapper_test",
    srcs = ["parser_field_mapper_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/non_extracted_header.ir.json",
        "testdata/parse_basic.pb.txt",
        "testdata/parse_value_set.ir.json",
        "testdata/value_set_parser_map.pb.txt",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4_parser_map_cc_proto",
        ":parser_decoder",
        ":parser_field_mapper",
        ":table_map_generator",
        ":table_map_generator_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "//stratum/lib:utils",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "parser_value_set_mapper",
    srcs = ["parser_value_set_mapper.cc"],
    hdrs = ["parser_value_set_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4_parser_map_cc_proto",
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

cc_test(
    name = "parser_value_set_mapper_test",
    srcs = ["parser_value_set_mapper_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/parse_value_set.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4_parser_map_cc_proto",
        ":parser_decoder",
        ":parser_value_set_mapper",
        ":table_map_generator",
        ":table_map_generator_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/glue/gtl:map_util",
    ],
)

cc_library(
    name = "pipeline_optimizer",
    srcs = ["pipeline_optimizer.cc"],
    hdrs = ["pipeline_optimizer.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":pipeline_block_passes",
        ":pipeline_intra_block_passes",
        "//stratum/glue:logging",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
    ],
)

cc_test(
    name = "pipeline_optimizer_test",
    srcs = ["pipeline_optimizer_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/pipeline_opt_inspect.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":pipeline_optimizer",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/p4c_backends/test:test_target_info",
    ],
)

cc_library(
    name = "pipeline_block_passes",
    srcs = ["pipeline_block_passes.cc"],
    hdrs = ["pipeline_block_passes.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/public/proto:p4_annotation_cc_proto",
    ],
)

cc_test(
    name = "pipeline_block_passes_test",
    srcs = ["pipeline_block_passes_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/pipeline_opt_block.ir.json",
        "testdata/pipeline_opt_inspect.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4c_switch_utils",
        ":pipeline_block_passes",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/p4c_backends/test:test_inspectors",
        "//stratum/p4c_backends/test:test_target_info",
    ],
)

cc_library(
    name = "pipeline_intra_block_passes",
    srcs = ["pipeline_intra_block_passes.cc"],
    hdrs = ["pipeline_intra_block_passes.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/public/proto:p4_annotation_cc_proto",
    ],
)

cc_test(
    name = "pipeline_intra_block_passes_test",
    srcs = ["pipeline_intra_block_passes_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/pipeline_intra_block1.ir.json",
        "testdata/pipeline_intra_block2.ir.json",
        "testdata/pipeline_intra_block3.ir.json",
        "testdata/pipeline_intra_block4.ir.json",
        "testdata/pipeline_intra_block5.ir.json",
        "testdata/switch_case.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4c_switch_utils",
        ":pipeline_block_passes",
        ":pipeline_intra_block_passes",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/p4c_backends/test:test_inspectors",
        "//stratum/p4c_backends/test:test_target_info",
        "//stratum/public/proto:p4_annotation_cc_proto",
    ],
)

cc_library(
    name = "simple_hit_inspector",
    srcs = ["simple_hit_inspector.cc"],
    hdrs = ["simple_hit_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//stratum/glue:logging",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
    ],
)

cc_test(
    name = "simple_hit_inspector_test",
    srcs = ["simple_hit_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/complex_hits.ir.json",
        "testdata/control_apply_hit_miss_test.ir.json",
        "testdata/control_if_test.ir.json",
        "testdata/control_misc_test.ir.json",
        "testdata/meter_color_nested_ifs.ir.json",
        "testdata/no_table_tmp.ir.json",
        "testdata/switch_case.ir.json",
        "testdata/table_hit_tmp_valid.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":simple_hit_inspector",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "slice_cross_reference",
    srcs = ["slice_cross_reference.cc"],
    hdrs = ["slice_cross_reference.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":expression_inspector",
        ":p4c_switch_utils",
        ":sliced_field_map_cc_proto",
        "//stratum/glue:logging",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

cc_test(
    name = "slice_cross_reference_test",
    srcs = ["slice_cross_reference_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/slice_assignments.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4_model_names_cc_proto",
        ":slice_cross_reference",
        ":sliced_field_map_cc_proto",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_library(
    name = "switch_case_decoder",
    srcs = ["switch_case_decoder.cc"],
    hdrs = ["switch_case_decoder.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":field_name_inspector",
        ":method_call_decoder",
        ":p4_model_names_cc_proto",
        ":table_map_generator",
        "//stratum/glue:logging",
        "//stratum/lib:macros",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
    ],
)

cc_test(
    name = "switch_case_decoder_test",
    srcs = ["switch_case_decoder_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/switch_case.ir.json",
        "testdata/switch_case_errors.ir.json",
        "testdata/switch_case_errors2.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        ":switch_case_decoder",
        ":table_map_generator_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
        "//stratum/lib:utils",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "switch_case_decoder_mock",
    testonly = 1,
    hdrs = ["switch_case_decoder_mock.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":switch_case_decoder",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "switch_p4c_backend",
    srcs = [
        "switch_p4c_backend.cc",
    ],
    hdrs = [
        "switch_p4c_backend.h",
    ],
    copts = [
        "-fexceptions",
    ],
    data = [
        ":slice_map_files",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":action_decoder",
        ":annotation_mapper",
        ":control_inspector",
        ":field_cross_reference",
        ":field_decoder",
        ":field_name_inspector",
        ":header_path_inspector",
        ":header_valid_inspector",
        ":hidden_static_mapper",
        ":hidden_table_mapper",
        ":hit_assign_mapper",
        ":meta_key_mapper",
        ":meter_color_mapper",
        ":p4_model_names_cc_proto",
        ":p4_parser_map_cc_proto",
        ":p4c_switch_utils",
        ":parser_decoder",
        ":parser_field_mapper",
        ":parser_value_set_mapper",
        ":pipeline_optimizer",
        ":slice_cross_reference",
        ":sliced_field_map_cc_proto",
        ":switch_case_decoder",
        ":table_hit_inspector",
        ":table_map_generator",
        ":table_type_mapper",
        ":tunnel_optimizer_interface",
        ":tunnel_type_mapper",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/p4c_backends/common:backend_extension_interface",
        "//stratum/p4c_backends/common:p4c_front_mid_interface",
        "//stratum/p4c_backends/common:program_inspector",
    ],
)

cc_test(
    name = "switch_p4c_backend_test",
    srcs = ["switch_p4c_backend_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        # google only
        # "testdata/b4_p4.ir.json",
        # "testdata/fbr_s2_p4.ir.json",
        # "testdata/fbr_s3_p4.ir.json",
        # "testdata/middleblock_p4.ir.json",
        # "testdata/spine_p4.ir.json",
        # "testdata/tor_p4.ir.json",
        "testdata/design_doc_sample1.ir.json",
        ":parser_map_files",
        "//stratum/p4c_backends/test:testdata/simple_vlan_stack_16.ir.json",
        "testdata/parse_annotated_state.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":switch_p4c_backend",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/lib:utils",
        "//stratum/p4c_backends/common:p4c_common_mocks",
        "//stratum/p4c_backends/fpm/bcm:bcm_tunnel_optimizer",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/p4c_backends/test:test_target_info",
    ],
)

cc_library(
    name = "table_hit_inspector",
    srcs = ["table_hit_inspector.cc"],
    hdrs = ["table_hit_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":simple_hit_inspector",
        "//stratum/glue:logging",
        "//stratum/lib:macros",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
    ],
)

cc_test(
    name = "table_hit_inspector_test",
    srcs = ["table_hit_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/complex_hits.ir.json",
        "testdata/control_apply_hit_miss_test.ir.json",
        "testdata/control_if_test.ir.json",
        "testdata/control_misc_test.ir.json",
        "testdata/control_return.ir.json",
        "testdata/hidden_table1.ir.json",
        "testdata/meter_color_if_else.ir.json",
        "testdata/meter_color_nested_ifs.ir.json",
        "testdata/meter_colors.ir.json",
        "testdata/no_table_tmp.ir.json",
        "testdata/switch_case.ir.json",
        "testdata/table_hit_tmp_invalid.ir.json",
        "testdata/table_hit_tmp_valid.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4c_switch_utils",
        ":table_hit_inspector",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "table_map_generator",
    srcs = [
        "table_map_generator.cc",
    ],
    hdrs = [
        "table_map_generator.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_test(
    name = "table_map_generator_test",
    srcs = ["table_map_generator_test.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4_model_names_cc_proto",
        ":p4c_switch_utils",
        ":table_map_generator",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_library(
    name = "table_map_generator_mock",
    testonly = 1,
    hdrs = ["table_map_generator_mock.h"],
    deps = [
        ":table_map_generator",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "table_type_mapper",
    srcs = ["table_type_mapper.cc"],
    hdrs = ["table_type_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_test(
    name = "table_type_mapper_test",
    srcs = ["table_type_mapper_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/table_type_mapper_p4info.pb.txt",
        "testdata/table_type_mapper_p4pipeline.pb.txt",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4c_switch_utils",
        ":table_type_mapper",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_library(
    name = "target_info",
    srcs = [
        "target_info.cc",
    ],
    hdrs = [
        "target_info.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//stratum/glue:logging",
        "//stratum/public/proto:p4_annotation_cc_proto",
    ],
)

cc_test(
    name = "target_info_test",
    srcs = ["target_info_test.cc"],
    deps = [
        ":target_info",
        ":target_info_mock",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "//stratum/public/proto:p4_annotation_cc_proto",
    ],
)

cc_library(
    name = "target_info_mock",
    testonly = 1,
    hdrs = ["target_info_mock.h"],
    deps = [
        ":target_info",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "tunnel_optimizer_interface",
    textual_hdrs = [
        "tunnel_optimizer_interface.h",
    ],
    deps = [
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
    ],
)

cc_library(
    name = "tunnel_optimizer_mock",
    testonly = 1,
    hdrs = ["tunnel_optimizer_mock.h"],
    deps = [
        ":tunnel_optimizer_interface",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "tunnel_type_mapper",
    srcs = ["tunnel_type_mapper.cc"],
    hdrs = ["tunnel_type_mapper.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4c_switch_utils",
        "//stratum/glue:logging",
        "@com_google_absl//absl/strings",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_test(
    name = "tunnel_type_mapper_test",
    srcs = ["tunnel_type_mapper_test.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4c_switch_utils",
        ":table_map_generator",
        ":tunnel_type_mapper",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

cc_library(
    name = "p4c_switch_utils",
    srcs = [
        "utils.cc",
    ],
    hdrs = [
        "utils.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":p4_model_names_cc_proto",
        ":p4_parser_map_cc_proto",
        ":target_info",
        "//stratum/glue:logging",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "//stratum/hal/lib/p4:p4_control_cc_proto",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

cc_test(
    name = "p4c_switch_utils_test",
    srcs = ["utils_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/action_misc.ir.json",
        # "testdata/tor_p4.ir.json", # google only, replaced by below file
        "testdata/field_inspect_test.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":p4_model_names_cc_proto",
        ":p4_parser_map_cc_proto",
        ":p4c_switch_utils",
        ":table_map_generator",
        ":target_info_mock",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_ir",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "@com_google_protobuf//:protobuf",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/lib:utils",
        "//stratum/p4c_backends/test:ir_test_helpers",
        "//stratum/public/proto:p4_table_defs_cc_proto",
    ],
)

# Various mapping input files for building production P4PipelineConfig
# files, including annotations mapping and target parser definitions.
cc_library(
    name = "map_data_files",
    data = [
        "map_data/sliced_field_map.pb.txt",
        "map_data/standard_parser_map.pb.txt",
        "map_data/tor_field_map.pb.txt",
        "map_data/tor_table_map.pb.txt",
    ],
)

cc_binary(
    name = "p4c-fpm",
    srcs = ["p4c_switch_main.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":annotation_mapper",
        ":midend",
        ":switch_p4c_backend",
        ":table_map_generator",
        ":target_info",
        "//stratum/glue:logging",
        "//stratum/p4c_backends/common:backend_extension_interface",
        "//stratum/p4c_backends/common:backend_pass_manager",
        "//stratum/p4c_backends/common:p4c_front_mid_real",
        "//stratum/p4c_backends/fpm/bcm:bcm_target_info",
        "//stratum/p4c_backends/fpm/bcm:bcm_tunnel_optimizer",
    ],
)

filegroup(
    name = "test_p4_includes",
    srcs = [
        "testdata/control_stubs.p4",
        "testdata/simple_headers.p4",
    ],
)

p4c_save_ir(
    name = "action_assignments",
    src = "testdata/action_assignments.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/action_assignments.ir.json",
)

p4c_save_ir(
    name = "action_misc",
    src = "testdata/action_misc.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/action_misc.ir.json",
)

p4c_save_ir(
    name = "assigned_constants_test1",
    src = "testdata/assigned_constants_test1.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/assigned_constants_test1.ir.json",
)

p4c_save_ir(
    name = "assigned_constants_test2",
    src = "testdata/assigned_constants_test2.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/assigned_constants_test2.ir.json",
)

p4c_save_ir(
    name = "assigned_constants_test3",
    src = "testdata/assigned_constants_test3.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/assigned_constants_test3.ir.json",
)

p4c_save_ir(
    name = "assigned_constants_test4",
    src = "testdata/assigned_constants_test4.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/assigned_constants_test4.ir.json",
)

p4c_save_ir(
    name = "assigned_constants_test5",
    src = "testdata/assigned_constants_test5.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/assigned_constants_test5.ir.json",
)

p4c_save_ir(
    name = "assigned_constants_test6",
    src = "testdata/assigned_constants_test6.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/assigned_constants_test6.ir.json",
)

p4c_save_ir(
    name = "assigned_constants_test7",
    src = "testdata/assigned_constants_test7.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/assigned_constants_test7.ir.json",
)

p4c_save_ir(
    name = "complex_hits",
    src = "testdata/complex_hits.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/complex_hits.ir.json",
)

p4c_save_ir(
    name = "control_apply_hit_miss_test_p4",
    src = "testdata/control_apply_hit_miss_test.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/control_apply_hit_miss_test.ir.json",
)

p4c_save_ir(
    name = "control_hit_meter",
    src = "testdata/control_hit_meter.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/control_hit_meter.ir.json",
)

p4c_save_ir(
    name = "control_if_test_p4",
    src = "testdata/control_if_test.p4",
    out_ir = "testdata/control_if_test.ir.json",
)

p4c_save_ir(
    name = "control_misc_test_p4",
    src = "testdata/control_misc_test.p4",
    out_ir = "testdata/control_misc_test.ir.json",
)

p4c_save_ir(
    name = "control_misc_test2_p4",
    src = "testdata/control_misc_test2.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/control_misc_test2.ir.json",
)

p4c_save_ir(
    name = "control_return_p4",
    src = "testdata/control_return.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/control_return.ir.json",
)

p4c_save_ir(
    name = "design_doc_sample1",
    src = "testdata/design_doc_sample1.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/design_doc_sample1.ir.json",
)

p4c_save_ir(
    name = "field_inspect_p4",
    src = "testdata/field_inspect_test.p4",
    out_ir = "testdata/field_inspect_test.ir.json",
)

p4c_save_ir(
    name = "header_decode_advanced",
    src = "testdata/header_decode_advanced.p4",
    hdrs = [
        "testdata/simple_headers.p4",
    ],
    out_ir = "testdata/header_decode_advanced.ir.json",
)

p4c_save_ir(
    name = "header_decode_basic",
    src = "testdata/header_decode_basic.p4",
    out_ir = "testdata/header_decode_basic.ir.json",
)

p4c_save_ir(
    name = "header_decode_controller",
    src = "testdata/header_decode_controller.p4",
    hdrs = [
        "testdata/simple_headers.p4",
    ],
    out_ir = "testdata/header_decode_controller.ir.json",
)

p4c_save_ir(
    name = "header_decode_stack",
    src = "testdata/header_decode_stack.p4",
    out_ir = "testdata/header_decode_stack.ir.json",
)

p4c_save_ir(
    name = "header_valid_tests",
    src = "testdata/header_valid_tests.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/header_valid_tests.ir.json",
)

p4c_save_ir(
    name = "hidden_table1",
    src = "testdata/hidden_table1.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/hidden_table1.ir.json",
)

p4c_save_ir(
    name = "hit_assign",
    src = "testdata/hit_assign.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/hit_assign.ir.json",
)

p4c_save_ir(
    name = "if_color_test",
    src = "testdata/if_color_test.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/if_color_test.ir.json",
)

p4c_save_ir(
    name = "if_color_test_errors",
    src = "testdata/if_color_test_errors.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/if_color_test_errors.ir.json",
)

p4c_save_ir(
    name = "meter_colors",
    src = "testdata/meter_colors.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/meter_colors.ir.json",
)

p4c_save_ir(
    name = "meter_color_errors1",
    src = "testdata/meter_color_errors1.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/meter_color_errors1.ir.json",
)

p4c_save_ir(
    name = "meter_color_if_else",
    src = "testdata/meter_color_if_else.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/meter_color_if_else.ir.json",
)

p4c_save_ir(
    name = "meter_color_nested_ifs",
    src = "testdata/meter_color_nested_ifs.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/meter_color_nested_ifs.ir.json",
)

p4c_save_ir(
    name = "method_calls",
    src = "testdata/method_calls.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/method_calls.ir.json",
)

p4c_save_ir(
    name = "nested_metadata_test",
    src = "testdata/nested_metadata_test.p4",
    out_ir = "testdata/nested_metadata_test.ir.json",
)

p4c_save_ir(
    name = "no_table_tmp",
    src = "testdata/no_table_tmp.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/no_table_tmp.ir.json",
)

p4c_save_ir(
    name = "non_extracted_header",
    src = "testdata/non_extracted_header.p4",
    out_ir = "testdata/non_extracted_header.ir.json",
)

p4c_save_ir(
    name = "parse_basic",
    src = "testdata/parse_basic.p4",
    out_ir = "testdata/parse_basic.ir.json",
)

p4c_save_ir(
    name = "parse_complex",
    src = "testdata/parse_complex.p4",
    out_ir = "testdata/parse_complex.ir.json",
)

p4c_save_ir(
    name = "parse_annotated_state",
    src = "testdata/parse_annotated_state.p4",
    out_ir = "testdata/parse_annotated_state.ir.json",
)

p4c_save_ir(
    name = "parse_value_set",
    src = "testdata/parse_value_set.p4",
    out_ir = "testdata/parse_value_set.ir.json",
)

p4c_save_ir(
    name = "pipeline_opt_block",
    src = "testdata/pipeline_opt_block.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/pipeline_opt_block.ir.json",
)

p4c_save_ir(
    name = "pipeline_intra_block1",
    src = "testdata/pipeline_intra_block1.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/pipeline_intra_block1.ir.json",
)

p4c_save_ir(
    name = "pipeline_intra_block2",
    src = "testdata/pipeline_intra_block2.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/pipeline_intra_block2.ir.json",
)

p4c_save_ir(
    name = "pipeline_intra_block3",
    src = "testdata/pipeline_intra_block3.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/pipeline_intra_block3.ir.json",
)

p4c_save_ir(
    name = "pipeline_intra_block4",
    src = "testdata/pipeline_intra_block4.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/pipeline_intra_block4.ir.json",
)

p4c_save_ir(
    name = "pipeline_intra_block5",
    src = "testdata/pipeline_intra_block5.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/pipeline_intra_block5.ir.json",
)

p4c_save_ir(
    name = "pipeline_opt_inspect",
    src = "testdata/pipeline_opt_inspect.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/pipeline_opt_inspect.ir.json",
)

p4c_save_ir(
    name = "slice_assignments_ir",
    src = "testdata/slice_assignments.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/slice_assignments.ir.json",
)

p4c_save_ir(
    name = "switch_case",
    src = "testdata/switch_case.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/switch_case.ir.json",
)

p4c_save_ir(
    name = "switch_case_errors",
    src = "testdata/switch_case_errors.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/switch_case_errors.ir.json",
)

p4c_save_ir(
    name = "switch_case_errors2",
    src = "testdata/switch_case_errors2.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/switch_case_errors2.ir.json",
)

p4c_save_ir(
    name = "table_hit_tmp_invalid",
    src = "testdata/table_hit_tmp_invalid.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/table_hit_tmp_invalid.ir.json",
)

p4c_save_ir(
    name = "table_hit_tmp_valid",
    src = "testdata/table_hit_tmp_valid.p4",
    hdrs = [
        ":test_p4_includes",
    ],
    out_ir = "testdata/table_hit_tmp_valid.ir.json",
)

"""google only, because they depend on the orion p4 behaviour version. The source files are also not included in the google code drop
p4c_save_ir(
    name = "middleblock_p4_ir",
    src = "testdata/middleblock.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    out_ir = "testdata/middleblock_p4.ir.json",
)

p4c_save_ir(
    name = "spine_p4_ir",
    src = "testdata/spine.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    out_ir = "testdata/spine_p4.ir.json",
)

p4c_save_ir(
    name = "tor_p4_ir",
    src = "testdata/tor.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    out_ir = "testdata/tor_p4.ir.json",
)

p4c_save_ir(
    name = "b4_p4_ir",
    src = "testdata/b4.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    out_ir = "testdata/b4_p4.ir.json",
)

p4c_save_ir(
    name = "fbr_s2_p4_ir",
    src = "testdata/fbr_s2.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    out_ir = "testdata/fbr_s2_p4.ir.json",
)

p4c_save_ir(
    name = "fbr_s3_p4_ir",
    src = "testdata/fbr_s3.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    out_ir = "testdata/fbr_s3_p4.ir.json",
)
"""

filegroup(
    name = "annotation_map_files",
    srcs = [
        "map_data/tor_field_map.pb.txt",
        "map_data/tor_table_map.pb.txt",
    ],
)

filegroup(
    name = "parser_map_files",
    srcs = [
        "map_data/standard_parser_map.pb.txt",
    ],
)

filegroup(
    name = "slice_map_files",
    srcs = [
        "map_data/sliced_field_map.pb.txt",
    ],
)
