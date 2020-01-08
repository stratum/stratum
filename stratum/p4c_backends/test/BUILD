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

# This BUILD file provides common test support for the Stratum p4c backend.
# It also creates a simple p4c test binary with limited backend
# functionality, then uses it to test p4c with the comprehensive set of
# sample files in //p4lang_p4c.

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

cc_library(
    name = "ir_test_helpers",
    srcs = [
        "ir_test_helpers.cc",
    ],
    hdrs = [
        "ir_test_helpers.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//stratum/glue:logging",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:control_plane_h",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "//stratum/p4c_backends/common:backend_extension_interface",
        "//stratum/p4c_backends/common:backend_pass_manager",
        "//stratum/p4c_backends/common:p4c_front_mid_real",
        "//stratum/p4c_backends/common:program_inspector",
        "//stratum/p4c_backends/fpm:hit_assign_mapper",
        "//stratum/p4c_backends/fpm:meter_color_mapper",
    ],
)

cc_test(
    name = "ir_test_helpers_test",
    srcs = [
        "ir_test_helpers_test.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/simple_vlan_stack_16.ir.json",
        "//stratum/p4c_backends/fpm:testdata/hit_assign.ir.json",
        "//stratum/p4c_backends/fpm:testdata/meter_color_errors1.ir.json",
        "//stratum/p4c_backends/fpm:testdata/meter_colors.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":ir_test_helpers",
        "@com_google_googletest//:gtest_main",
        # "@com_github_p4lang_p4c//:p4c_testdata_files", # FIXME
        "//stratum/p4c_backends/fpm:p4c_switch_utils",
    ],
)

# Inspects IR nodes to collect data for unit tests.
cc_library(
    name = "test_inspectors",
    hdrs = [
        "test_inspectors.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//stratum/glue:logging",
        "@com_github_p4lang_p4c//:p4c_toolkit",
    ],
)

cc_test(
    name = "test_inspectors_test",
    srcs = [
        "test_inspectors_test.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    data = [
        "//stratum/p4c_backends/fpm:testdata/pipeline_opt_block.ir.json",
    ],
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    size = "small",
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":ir_test_helpers",
        ":test_inspectors",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "//stratum/p4c_backends/fpm:p4c_switch_utils",
        "//stratum/public/proto:p4_annotation_cc_proto",
    ],
)

cc_library(
    name = "test_target_info",
    srcs = [
        "test_target_info.cc",
    ],
    hdrs = [
        "test_target_info.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//stratum/p4c_backends/fpm:target_info",
    ],
)

cc_test(
    name = "test_target_info_test",
    srcs = [
        "test_target_info_test.cc",
    ],
    size = "small",
    copts = [
        "-fexceptions",
    ],
    data = [
        "//stratum/p4c_backends/fpm:testdata/pipeline_opt_block.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":test_target_info",
        "@com_google_googletest//:gtest_main",
        "//stratum/public/proto:p4_annotation_cc_proto",
    ],
)

# This p4c binary generates IR data in JSON format for unit tests.
cc_binary(
    name = "p4c_ir_json_saver",
    srcs = [
        "p4c_ir_json_saver.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        "//stratum/glue:logging",
        "//stratum/p4c_backends/common:backend_extension_interface",
        "//stratum/p4c_backends/common:backend_pass_manager",
        "//stratum/p4c_backends/common:p4c_front_mid_real",
        "//stratum/p4c_backends/fpm:midend",
    ],
)

# This p4c binary runs from the p4c_sample_files_test.
cc_binary(
    name = "test_p4c",
    srcs = [
        "test_p4c_main.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        "//stratum/glue:logging",
        "//stratum/p4c_backends/common:backend_extension_interface",
        "//stratum/p4c_backends/common:backend_pass_manager",
        "//stratum/p4c_backends/common:p4c_front_mid_real",
    ],
)

# This test must execute with gunit_main in order to access the test file
# lists through FLAGS_test_srcdir.  For this to work, it needs to avoid
# direct or indirect sandblaze dependencies.  The local flag is set because
# p4c forks the C preprocessor, which isn't available in the blaze test
# environment.
cc_test(
    name = "p4c_sample_files_test",
    size = "medium",
    timeout = "moderate",
    srcs = ["p4c_sample_files_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        ":test_p4c",
        "@com_github_p4lang_p4c//:testdata_p4_16_samples",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    local = 1,
    deps = [
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        # "//p4lang_p4c:p4_16_test_file_list", # FIXME
        # "//p4lang_p4c:p4c_testdata_files",
    ],
)

p4c_save_ir(
    name = "simple_vlan_stack",
    src = "testdata/simple_vlan_stack_16.p4",
    out_ir = "testdata/simple_vlan_stack_16.ir.json",
)
