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
    "HOST_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_cc_lib(
    name = "onlp_event_handler",
    srcs = ["onlp_event_handler.cc"],
    hdrs = ["onlp_event_handler.h"],
    deps = [
        ":onlp_wrapper",
        "//base",
        "//absl/container:flat_hash_map",
        "//absl/synchronization",
        "//absl/time",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/lib:macros",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "onlp_event_handler_test",
    srcs = ["onlp_event_handler_test.cc"],
    deps = [
        ":onlp_event_handler",
        ":onlp_wrapper_mock",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/synchronization",
        "//absl/time",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "onlp_wrapper",
    srcs = [
        "onlp_wrapper.cc",
    ],
    hdrs = [
        "onlp_wrapper.h",
    ],
    textual_hdrs = [
        #       TODO: Fix dependency
        #        "//stratum/onlp:onlp_hdrs",
    ],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/lib:macros",
        "//absl/memory",
        "//absl/synchronization",
        #       TODO: Fix dependency
        #        "//stratum/onlp:onlpsim",
    ],
)

sc_cc_lib(
    name = "onlp_wrapper_mock",
    testonly = 1,
    hdrs = [
        "onlp_wrapper_mock.h",
    ],
    arches = HOST_ARCHES,
    deps = [
        ":onlp_wrapper",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

sc_cc_lib(
    name = "sfp_datasource",
    srcs = [
        "sfp_datasource.cc",
    ],
    hdrs = [
        "sfp_datasource.h",
    ],
    deps = [
        ":onlp_wrapper",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_proto",
        "//stratum/hal/lib/phal:system_interface",
        "//stratum/lib:macros",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/synchronization",
        #       TODO: Fix dependency
        #        "//stratum/onlp:onlpsim",
    ],
)

sc_cc_test(
    name = "sfp_datasource_test",
    srcs = [
        "sfp_datasource_test.cc",
    ],
    deps = [
        ":onlp_wrapper_mock",
        ":sfp_datasource",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_proto",
        "//stratum/hal/lib/phal:system_interface",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        #       TODO: Fix dependency
        #        "//stratum/onlp:onlpsim",
    ],
)
