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
    "//third_party/stratum/portage:build_defs.bzl",
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
        "//third_party/absl/base:core_headers",
    ],
)

sc_cc_lib(
    name = "macros",
    hdrs = ["macros.h"],
    deps = [
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "macros_test",
    srcs = ["macros_test.cc"],
    deps = [
        ":macros",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/stratum/glue/status:status_test_util",
    ],
)

cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/glue:init_google",
        "//third_party/stratum/glue:logging",
    ],
)

sc_cc_lib(
    name = "timer_daemon",
    srcs = ["timer_daemon.cc"],
    hdrs = ["timer_daemon.h"],
    deps = [
        "//third_party/absl/synchronization",
        "//third_party/absl/time",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "timer_daemon_test",
    srcs = ["timer_daemon_test.cc"],
    deps = [
        ":test_main",
        ":timer_daemon",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":macros",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/strings",
        "//third_party/protobuf",
        "//third_party/sandblaze/prebuilt/grpc",
        "//third_party/stratum:code_proto",
        "//third_party/stratum:status_proto",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/public/lib:error",
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
        "//third_party/sandblaze/p4lang:p4runtime_grpc_proto",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/public/lib:error",
        "//third_party/stratum/public/proto:hal_grpc_proto",
    ],
)
