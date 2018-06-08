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
    "//stratum/portage:build_defs.bzl",
    "STRATUM_INTERNAL",
    "sc_cc_lib",
    "sc_cc_test",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_cc_lib(
    name = "constants",
    hdrs = ["constants.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
    ],
)

sc_cc_lib(
    name = "macros",
    hdrs = ["macros.h"],
    deps = [
        "//stratum/glue/status:status_macros",
        "//stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "macros_test",
    srcs = ["macros_test.cc"],
    deps = [
        ":macros",
        "//testing/base/public:gunit_main_no_google3",
        "//stratum/glue/status:status_test_util",
    ],
)

cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)

sc_cc_lib(
    name = "timer_daemon",
    srcs = ["timer_daemon.cc"],
    hdrs = ["timer_daemon.h"],
    deps = [
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "timer_daemon_test",
    srcs = ["timer_daemon_test.cc"],
    deps = [
        ":test_main",
        ":timer_daemon",
        "//testing/base/public:gunit_no_google3",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "//protobuf",
#        "//sandblaze/prebuilt/grpc",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum:code_proto",
        "//stratum:status_proto",
        "//stratum/glue/status",
        "//stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//sandblaze/p4lang:p4runtime_grpc_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/public/lib:error",
        "//stratum/public/proto:hal_grpc_proto",
    ],
)
