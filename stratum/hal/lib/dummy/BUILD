#
# Copyright 2018-present Open Networking Foundation.
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

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("//bazel:defs.bzl", "HOST_ARCHES", "STRATUM_INTERNAL")
load("//bazel/rules:library_rule.bzl", "stratum_cc_library")
#load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "dummy_global_vars",
    srcs = ["dummy_global_vars.cc"],
    hdrs = ["dummy_global_vars.h"],
    deps = [
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "dummy_node",
    srcs = ["dummy_node.cc"],
    hdrs = ["dummy_node.h"],
    deps = ["@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
            "@com_google_absl//absl/synchronization",
            "@com_google_absl//absl/container:flat_hash_map",
            "//stratum/glue:integral_types",
            "//stratum/glue:logging",
            "//stratum/glue/status:status_macros",
            "//stratum/hal/lib/common:writer_interface",
            "//stratum/hal/lib/common:common_cc_proto",
            "//stratum/public/lib:error",
            ":dummy_box",
            ":dummy_global_vars",
    ]
)

stratum_cc_library(
    name = "dummy_phal",
    srcs = ["dummy_phal.cc"],
    hdrs = ["dummy_phal.h"],
    deps = ["@com_google_absl//absl/synchronization",
            "//stratum/glue:logging",
            "//stratum/glue:integral_types",
            "//stratum/hal/lib/common:writer_interface",
            "//stratum/hal/lib/common:phal_interface",
            "//stratum/hal/lib/common:constants",
            ":dummy_box",
            ":dummy_global_vars",
    ]
)

stratum_cc_library(
    name = "dummy_chassis_mgr",
    srcs = ["dummy_chassis_mgr.cc"],
    hdrs = ["dummy_chassis_mgr.h"],
    deps = ["@com_google_absl//absl/synchronization",
            "@com_google_absl//absl/strings:str_format",
            "@com_google_absl//absl/container:flat_hash_map",
            "//stratum/hal/lib/common:switch_interface",
            ":dummy_node",
            ":dummy_box",
            ":dummy_global_vars",
    ]
)

stratum_cc_library(
    name = "dummy_switch",
    srcs = ["dummy_switch.cc"],
    hdrs = ["dummy_switch.h"],
    deps = ["@com_google_absl//absl/synchronization",
            "@com_google_absl//absl/container:flat_hash_map",
            "//stratum/hal/lib/common:switch_interface",
            "//stratum/hal/lib/common:phal_interface",
            ":dummy_chassis_mgr",
            ":dummy_global_vars",
    ],
)

proto_library(
    name = "dummy_test_proto",
    srcs = ["dummy_test.proto"],
    deps = ["//stratum/hal/lib/common:common_proto"]
)

cc_proto_library(
    name = "dummy_test_cc_proto",
    deps = [":dummy_test_proto"]
)

cc_grpc_library(
    name = "dummy_test_cc_grpc",
    srcs = [":dummy_test_proto"],
    deps = [":dummy_test_cc_proto"],
    grpc_only = True,
)

#py_proto_library(
#    name = "dummy_test_py_grpc",
#    deps = [
#        ":dummy_test_proto"
#    ],
#    well_known_protos = False,
#)

stratum_cc_library(
    name = "dummy_box",
    srcs = ["dummy_box.cc"],
    hdrs = ["dummy_box.h"],
    deps = [
        "@com_google_absl//absl/time:time",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue/status:status",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:common_cc_proto",
        ":dummy_test_cc_proto",
        ":dummy_test_cc_grpc",
    ]
)
