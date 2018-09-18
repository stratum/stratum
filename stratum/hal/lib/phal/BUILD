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
    "sc_cc_bin",
    "sc_cc_lib",
    "sc_cc_test",
    "sc_package",
    "sc_proto_lib",
    "EMBEDDED_ARCHES",
    "HOST_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_cc_lib(
    name = "attribute_database_interface",
    hdrs = ["attribute_database_interface.h"],
    deps = [
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/types:variant",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/phal:db_proto",
        "//stratum/lib/channel",
        "//util/gtl:flat_hash_map",
        "//util/hash:mix",
    ],
)

sc_cc_lib(
    name = "attribute_database_mock",
    testonly = 1,
    hdrs = ["attribute_database_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_database_interface",
        "//testing/base/public:gunit_no_google3",
        "//util/gtl:flat_hash_map",
    ],
)

sc_cc_lib(
    name = "buffer_tools",
    hdrs = ["buffer_tools.h"],
    deps = [
        "//absl/base:core_headers",
    ],
)

sc_cc_lib(
    name = "managed_attribute",
    hdrs = ["managed_attribute.h"],
    deps = [
        ":attribute_database_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "managed_attribute_mock",
    testonly = 1,
    hdrs = ["managed_attribute_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_database_interface",
        ":datasource",
        ":managed_attribute",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status",
    ],
)

sc_cc_lib(
    name = "attribute_database",
    srcs = ["attribute_database.cc"],
    hdrs = ["attribute_database.h"],
    deps = [
        ":attribute_database_interface",
        ":attribute_group",
        ":datasource",
        ":db_proto",
        ":dummy_threadpool",
        ":managed_attribute",
        ":phal_proto",
        ":system_interface",
        ":threadpool_interface",
        ":udev_event_handler",
        "//absl/container:flat_hash_set",
        "//absl/memory",
        "//absl/synchronization",
        "//absl/time",
        "//protobuf",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
    ],
)

sc_cc_test(
    name = "attribute_database_test",
    srcs = ["attribute_database_test.cc"],
    data = [
        "//platforms/networking/hercules/hal/config:legacy_phal_init_config_generic_trident2_hercules.pb.txt",
    ],
    deps = [
        "system_fake",
        ":attribute_database",
        ":attribute_group_mock",
        ":db_proto",
        ":dummy_threadpool",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//absl/synchronization",
        "//absl/time",
        "//protobuf",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "attribute_group",
    srcs = ["attribute_group.cc"],
    hdrs = ["attribute_group.h"],
    deps = [
        ":attribute_database_interface",
        ":datasource",
        ":managed_attribute",
        ":threadpool_interface",
        "//absl/container:flat_hash_map",
        "//absl/memory",
        "//absl/synchronization",
        "//absl/time",
        "//protobuf",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "attribute_group_test",
    srcs = ["attribute_group_test.cc"],
    deps = [
        ":attribute_group",
        ":datasource",
        ":datasource_mock",
        ":dummy_threadpool",
        ":managed_attribute",
        ":managed_attribute_mock",
        ":test_util",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/base:core_headers",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/phal/test:test_proto",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "attribute_group_mock",
    testonly = 1,
    srcs = ["attribute_group_mock.cc"],
    hdrs = ["attribute_group_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_group",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//util/gtl:flat_hash_map",
    ],
)

sc_cc_lib(
    name = "datasource",
    srcs = ["datasource.cc"],
    hdrs = ["datasource.h"],
    deps = [
        ":attribute_database_interface",
        ":managed_attribute",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/synchronization",
        "//absl/time",
        "//stratum/glue/status",
    ],
)

sc_cc_lib(
    name = "datasource_mock",
    testonly = 1,
    srcs = ["datasource_mock.cc"],
    hdrs = ["datasource_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":datasource",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status",
    ],
)

sc_proto_lib(
    name = "db_proto",
    hdrs = ["db.proto"],
    python_support = True,
    deps = [
        "//stratum/hal/lib/common:common_proto",
    ],
)

sc_cc_lib(
    name = "dummy_threadpool",
    srcs = ["dummy_threadpool.cc"],
    hdrs = ["dummy_threadpool.h"],
    deps = [
        ":threadpool_interface",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//stratum/glue/status",
    ],
)

sc_cc_lib(
    name = "filepath_stringsource",
    hdrs = ["filepath_stringsource.h"],
    deps = [
        ":stringsource_interface",
        ":system_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
    ],
)

sc_cc_lib(
    name = "fixed_layout_datasource",
    srcs = ["fixed_layout_datasource.cc"],
    hdrs = ["fixed_layout_datasource.h"],
    deps = [
        ":buffer_tools",
        ":datasource",
        ":managed_attribute",
        ":stringsource_interface",
        "//absl/memory",
        "//stratum/lib:macros",
    ],
)

sc_cc_test(
    name = "fixed_layout_datasource_test",
    srcs = ["fixed_layout_datasource_test.cc"],
    deps = [
        ":fixed_layout_datasource",
        ":fixed_stringsource",
        ":test_util",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/phal/test:test_proto",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "fixed_stringsource",
    hdrs = ["fixed_stringsource.h"],
    deps = [
        ":stringsource_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "google_phal",
    srcs = ["google_phal.cc"],
    hdrs = ["google_phal.h"],
    deps = [
        ":attribute_database",
        ":attribute_database_interface",
        ":system_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//absl/time",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
    ],
)

sc_cc_test(
    name = "google_phal_test",
    srcs = ["google_phal_test.cc"],
    deps = [
        ":attribute_database_mock",
        ":google_phal",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//absl/synchronization",
        "//absl/time",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_bin(
    name = "phal_cli",
    srcs = ["phal_cli.cc"],
    arches = EMBEDDED_ARCHES,
    copts = [
        "-lpthread",
        "-ldl",
        "-lrt",
    ],
    deps = [
        ":attribute_database",
        ":attribute_database_interface",
        ":system_real",
        "//base",
        "//absl/strings",
        "//absl/time",
        "//stratum/glue:init_google",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//util/regexp/re2",
        "//util/task:status",
    ],
)

sc_package(
    name = "phal_cli_pkg",
    arches = EMBEDDED_ARCHES,
    bins = [":phal_cli"],
)

sc_cc_lib(
    name = "reader_writer_datasource",
    hdrs = ["reader_writer_datasource.h"],
    deps = [
        ":datasource",
        ":stringsource_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

cc_test(
    name = "reader_writer_datasource_test",
    srcs = ["reader_writer_datasource_test.cc"],
    deps = [
        ":filepath_stringsource",
        ":reader_writer_datasource",
        ":system_fake",
        ":test_util",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "regex_datasource",
    srcs = ["regex_datasource.cc"],
    hdrs = ["regex_datasource.h"],
    deps = [
        ":datasource",
        ":managed_attribute",
        ":stringsource_interface",
        "//base",
        "//absl/memory",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//util/regexp/re2",
    ],
)

sc_cc_test(
    name = "regex_datasource_test",
    srcs = ["regex_datasource_test.cc"],
    deps = [
        ":datasource",
        ":fixed_stringsource",
        ":regex_datasource",
        ":test_util",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "stringsource_interface",
    hdrs = ["stringsource_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

sc_cc_lib(
    name = "system_fake",
    testonly = 1,
    srcs = ["system_fake.cc"],
    hdrs = ["system_fake.h"],
    arches = HOST_ARCHES,
    deps = [
        ":system_interface",
        "//absl/memory",
        "//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "system_interface",
    hdrs = ["system_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

sc_cc_lib(
    name = "system_real",
    srcs = ["system_real.cc"],
    hdrs = ["system_real.h"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":system_interface",
        "//absl/memory",
        "//absl/synchronization",
        "//sandblaze/prebuilt/eudev",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
    ],
)

sc_cc_lib(
    name = "system_interface_mock",
    testonly = 1,
    hdrs = ["system_interface_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":system_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_lib(
    name = "test_util",
    testonly = 1,
    hdrs = ["test_util.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_database_interface",
        ":datasource",
        ":managed_attribute",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "threadpool_interface",
    hdrs = ["threadpool_interface.h"],
    deps = [
        "//absl/base:core_headers",
        "//stratum/glue/status",
    ],
)

sc_cc_lib(
    name = "threadpool_interface_mock",
    testonly = 1,
    hdrs = ["threadpool_interface_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":threadpool_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_lib(
    name = "udev_event_handler",
    srcs = ["udev_event_handler.cc"],
    hdrs = ["udev_event_handler.h"],
    deps = [
        ":system_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "udev_event_handler_mock",
    testonly = 1,
    hdrs = ["udev_event_handler_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":udev_event_handler",
        "//testing/base/public:gunit_main_no_google3",
    ],
)

sc_cc_test(
    name = "udev_event_handler_test",
    srcs = ["udev_event_handler_test.cc"],
    deps = [
        ":system_fake",
        ":udev_event_handler",
        ":udev_event_handler_mock",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "legacy_phal",
    srcs = ["legacy_phal.cc"],
    hdrs = ["legacy_phal.h"],
    deps = [
        ":udev_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:phal_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//util/gtl:stl_util",
    ],
)

sc_proto_lib(
    name = "phal_proto",
    hdrs = ["phal.proto"],
    python_support = True,
    deps = [
        "//stratum/hal/lib/common:common_proto",
    ],
)

sc_cc_lib(
    name = "phal_proxy",
    srcs = ["phal_proxy.cc"],
    hdrs = ["phal_proxy.h"],
    deps = [
        ":phal_proto",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/hal/lib/common:client_sync_reader_writer",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib/sandcastle:hardware_status_service_grpc_proto",
        "//stratum/public/proto:error_proto",
    ],
)

sc_cc_lib(
    name = "phal_sim",
    srcs = ["phal_sim.cc"],
    hdrs = ["phal_sim.h"],
    deps = [
        ":phal_proto",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "udev",
    srcs = ["udev.cc"],
    hdrs = ["udev.h"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":udev_interface",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/synchronization",
        "//sandblaze/prebuilt/eudev",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "udev_interface",
    hdrs = ["udev_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

sc_cc_lib(
    name = "udev_mock",
    testonly = 1,
    hdrs = ["udev_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":udev_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)
