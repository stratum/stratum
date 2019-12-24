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
    srcs = ["taimanager.cc",
            "taiadapterhost.cc",
            "taiobject.cc",
            "networkinterface.cc",
            "hostinterface.cc",
            "module.cc",
            "typesconverter.cc",
            "taiattribute.cc"],
    hdrs = ["taimanager.h",
            "taiadapterhost.h",
            "taiobject.h",
            "networkinterface.h",
            "hostinterface.h",
            "module.h",
            "typesconverter.h",
            "taiattribute.h"],
    deps = [
        "@com_github_oopt_tai//:metatai",
        # taimanager.h/.cc dependencies
        "//stratum/lib:utils",
        "//stratum/hal/lib/common:gnmi_events_lib",
        "//stratum/hal/lib/common:common_cc_proto",
    ],
)

stratum_cc_test(
    name = "tai_tests",
    srcs = [
        "test/tai_interface_test.cc",
        "test/tai_manager_test.cc"
    ],

    deps = [
        "//stratum/hal/lib/common:test_main",
        "//stratum/hal/lib/dummy:dummy_tai",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ]
)

