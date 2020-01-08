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
    "stratum_cc_library",
    "STRATUM_INTERNAL",
)

load("//bazel/rules:library_rule.bzl", "stratum_cc_library")
load("//bazel:defs.bzl", "STRATUM_INTERNAL")

package(
#    default_copts = [
#        "-DDISABLE_GOOGLE_GLOBAL_USING_DECLARATIONS",
#    ],
#    default_hdrs_check = "warn",
    default_visibility = STRATUM_INTERNAL,
)

cc_library(
    name = "p4c_front_mid_interface",
    textual_hdrs = [
        "p4c_front_mid_interface.h",
    ],
)

cc_library(
    name = "backend_extension_interface",
    textual_hdrs = [
        "backend_extension_interface.h",
    ],
    deps = [
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
    ],
)

cc_library(
    name = "midend_interface",
    textual_hdrs = [
        "midend_interface.h",
    ],
)

cc_library(
    name = "p4c_common_mocks",
    textual_hdrs = [
        "backend_extension_mock.h",
        "p4c_front_mid_mock.h",
    ],
)

cc_library(
    name = "p4c_front_mid_real",
    srcs = [
        "p4c_front_mid_real.cc",
    ],
    hdrs = [
        "p4c_front_mid_real.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":midend_interface",
        ":midend_p4c_open",
        ":p4c_front_mid_interface",
        "//stratum/glue:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:control_plane",
        "@com_github_p4lang_p4c//:control_plane_h",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_toolkit",
    ],
)

cc_library(
    name = "backend_pass_manager",
    srcs = [
        "backend_pass_manager.cc",
    ],
    hdrs = [
        "backend_pass_manager.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":backend_extension_interface",
        ":p4c_front_mid_interface",
        "//stratum/glue:logging",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "@com_github_p4lang_p4c//:p4c_backend_p4test_lib",
        "@com_github_p4lang_p4c//:p4c_toolkit",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/lib:utils",
        "//stratum/lib:macros",
    ],
)

cc_test(
    name = "backend_pass_manager_test",
    srcs = ["backend_pass_manager_test.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":backend_pass_manager",
        ":p4c_common_mocks",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
    ],
)

cc_library(
    name = "midend_p4c_open",
    srcs = ["midend_p4c_open.cc"],
    hdrs = ["midend_p4c_open.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":midend_interface",
        "//stratum/glue:logging",
        "//stratum/lib:macros",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        #"@com_github_p4lang_p4c//backends/p4test:p4c_common_midend_pass",
        "@com_github_p4lang_p4c//:p4c_backend_p4test_lib",
    ],
)

# TODO
cc_test(
    name = "midend_p4c_open_test",
    srcs = ["midend_p4c_open_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "//stratum/p4c_backends/test:testdata/simple_vlan_stack_16.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":midend_p4c_open",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)

cc_library(
    name = "program_inspector",
    srcs = ["program_inspector.cc"],
    hdrs = ["program_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/glue:logging",
    ],
)

cc_test(
    name = "program_inspector_test",
    srcs = ["program_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        # "//stratum/p4c_backends/fpm:testdata/tor_p4.ir.json", # google only, replaced by below file
        "//stratum/p4c_backends/fpm:testdata/field_inspect_test.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    linkopts = [
        "-lgmp",
        "-lgmpxx",
    ],
    deps = [
        ":program_inspector",
        "//stratum/glue:logging",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_github_p4lang_p4c//:p4c_frontend_midend",
        "@com_github_p4lang_p4c//:p4c_ir",
        "//stratum/p4c_backends/test:ir_test_helpers",
    ],
)
