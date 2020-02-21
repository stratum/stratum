#
# Copyright 2020-present Open Networking Foundation
# Copyright 2020 PLVision
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

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "HOST_ARCHES",
)

package(
    default_visibility = STRATUM_INTERNAL,
)

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

stratum_cc_library(
    name = "tai_primitives",
    srcs = [
        "tai_object.cc",
        "network_interface.cc",
        "host_interface.cc",
        "module.cc",
        "tai_attribute.cc",
    ],
    hdrs = [
        "tai_object.h",
        "network_interface.h",
        "host_interface.h",
        "module.h",
        "tai_attribute.h",
    ],
    deps = [
        "@com_github_oopt_tai//:libmetatai.so",
        "@com_github_oopt_tai//:tai_hdrs",
        "@com_github_oopt_tai//:meta_hdrs",
        "@com_github_oopt_tai//:libtai-stub.so",
        "//stratum/lib:utils",
    ]
)

stratum_cc_library(
    name = "tai_wrapper_interface",
    srcs = [
        "tai_wrapper_interface.cc"
    ],
    hdrs = [
        "tai_wrapper_interface.h"
    ],
    deps = [
        ":tai_primitives"
    ]
)

stratum_cc_library(
    name = "tai_wrapper",
    srcs = [
        "tai_wrapper.cc",
    ],
    hdrs = [
        "tai_wrapper.h",
    ],
    deps = [
        ":tai_wrapper_interface",
    ]
)

stratum_cc_library(
    name = "tai_types_converter",
    srcs = [
        "tai_types_converter.cc",
    ],
    hdrs = [
        "tai_types_converter.h",
    ],
    deps = [
        ":tai_primitives"
    ]
)

stratum_cc_library(
    name = "tai_manager",
    srcs = [
        "tai_manager.cc",
    ],
    hdrs = [
        "tai_manager.h",
    ],
    deps = [
        ":tai_wrapper",
        ":tai_types_converter",
        "//stratum/lib:utils",
        "//stratum/hal/lib/common:common_cc_proto",
    ],
)

stratum_cc_test(
    name = "tai_tests",
    srcs = [
        "tai_interface_test.cc",
        "tai_manager_test.cc",
        "tai_wrapper_mock.h",
        "tai_object_mock.h",
    ],
    deps = [
        ":tai_manager",
        ":tai_wrapper_interface",
        ":tai_primitives",
        "//stratum/hal/lib/common:test_main",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_github_oopt_tai//:libmetatai",
        "@com_github_oopt_tai//:libtai-stub",
        "@com_github_oopt_tai//:tai_hdrs",
        "@com_github_oopt_tai//:meta_hdrs",
        "//stratum/lib:utils",
        "//stratum/hal/lib/common:common_cc_proto",
    ],
)

stratum_cc_library(
    name = "taish_client",
    srcs = ["taish_client.cc"],
    hdrs = ["taish_client.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_oopt_tai_taish_proto//:taish_cc_grpc",
        "//stratum/lib:macros",
        "//stratum/glue:integral_types",
        "@com_google_absl//absl/strings",
    ],
)
