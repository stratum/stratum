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

exports_files(["LICENSE"])

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_cc_proto_library",
    "HOST_ARCHES",
)

load("@org_pubref_rules_protobuf//python:rules.bzl", "py_proto_library")

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "dummy_node",
    srcs = ["dummy_node.cc"],
    hdrs = ["dummy_node.h"],
    deps = ["@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
            "@com_google_absl//absl/synchronization",
            "//stratum/glue:integral_types",
            "//stratum/glue:logging",
            "//stratum/glue/gtl:flat_hash_map",
            "//stratum/glue/status:status_macros",
            "//stratum/hal/lib/common:writer_interface",
            "//stratum/hal/lib/common:common_proto",
            "//stratum/public/lib:error",
            ":dummy_sdk"
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
            ":dummy_sdk",
    ]
)

stratum_cc_library(
    name = "dummy_chassis_mgr",
    srcs = ["dummy_chassis_mgr.cc"],
    hdrs = ["dummy_chassis_mgr.h"],
    deps = ["@com_google_absl//absl/synchronization",
            "@com_google_absl//absl/strings:str_format",
            "//stratum/hal/lib/common:switch_interface",
            ":dummy_node",
            ":dummy_sdk",
    ]
)

stratum_cc_library(
    name = "dummy_switch",
    srcs = ["dummy_switch.cc"],
    hdrs = ["dummy_switch.h"],
    deps = ["@com_google_absl//absl/synchronization",
            "//stratum/hal/lib/common:switch_interface",
            "//stratum/hal/lib/common:phal_interface",
            ":dummy_chassis_mgr"
    ],
)

stratum_cc_proto_library(
    name = "dummy_test_proto",
    hdrs = ["dummy_test.proto"],
    deps = ["//stratum/public/proto/openconfig:openconfig_proto",
            "//stratum/hal/lib/common:common_proto"
    ]
)

py_proto_library(
    name = "dummy_test_py_proto",
    protos = ["dummy_test.proto"],
    proto_deps = [
        "//stratum/hal/lib/common:common_py_proto"
    ]
)

stratum_cc_library(
    name = "dummy_sdk",
    srcs = ["dummy_sdk.cc"],
    hdrs = ["dummy_sdk.h"],
    deps = ["@com_google_absl//absl/time:time",
            "@com_google_absl//absl/synchronization",
            "//stratum/glue/status:status",
            "//stratum/hal/lib/common:switch_interface",
            "//stratum/hal/lib/common:phal_interface",
            "//stratum/hal/lib/common:common_proto",
            ":dummy_test_proto"]
)
