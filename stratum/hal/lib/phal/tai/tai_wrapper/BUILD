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
    name = "tai",
    srcs = [
        "tai_manager.cc",
        "tai_wrapper.cc",
        "tai_object.cc",
        "network_interface.cc",
        "host_interface.cc",
        "module.cc",
        "types_converter.cc",
        "tai_attribute.cc",
        "tai_wrapper_interface.cc",
    ],
    hdrs = [
        "tai_manager.h",
        "tai_wrapper.h",
        "tai_object.h",
        "network_interface.h",
        "host_interface.h",
        "module.h",
        "types_converter.h",
        "tai_attribute.h",
        "tai_wrapper_interface.h",
    ],
    deps = [
        "@com_github_tai_implementations//tai_mux:libtai-mux",
        "@com_github_oopt_tai//:libmetatai",
        "@com_github_oopt_tai//:tai_hdrs",
        "@com_github_oopt_tai//:meta_hdrs",
        # tai_manager.h/.cc dependencies
        "//stratum/lib:utils",
        "//stratum/hal/lib/common:gnmi_events_lib",
        "//stratum/hal/lib/common:common_cc_proto",
    ],
)

stratum_cc_test(
    name = "tai_tests",
    srcs = [
        "test/tai_interface_test.cc",
        "test/tai_manager_test.cc",
        "test/tai_wrapper_mock.h",
        "test/tai_test_manager_wrapper.h",
        "test/tai_object_mock.h",

        "tai_manager.cc",
        "tai_wrapper.cc",
        "tai_object.cc",
        "network_interface.cc",
        "host_interface.cc",
        "module.cc",
        "types_converter.cc",
        "tai_attribute.cc",
        "tai_wrapper_interface.cc",

        "tai_manager.h",
        "tai_wrapper.h",
        "tai_object.h",
        "network_interface.h",
        "host_interface.h",
        "module.h",
        "types_converter.h",
        "tai_attribute.h",
        "tai_wrapper_interface.h",
    ],

    deps = [
        "//stratum/hal/lib/common:test_main",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",

        "@com_github_oopt_tai//:libmetatai",
        "@com_github_oopt_tai//:libtai-stub",
        "@com_github_oopt_tai//:tai_hdrs",
        "@com_github_oopt_tai//:meta_hdrs",
        # taimanager.h/.cc dependencies
        "//stratum/lib:utils",
        "//stratum/hal/lib/common:gnmi_events_lib",
        "//stratum/hal/lib/common:common_cc_proto",
    ]
)

