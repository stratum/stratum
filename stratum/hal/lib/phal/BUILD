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
    "sc_cc_bin",
    "sc_cc_lib",
    "sc_cc_test",
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
        "//third_party/absl/base:core_headers",
        "//third_party/absl/types:variant",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/phal:db_proto",
        "//third_party/stratum/lib/channel",
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
    ],
)

sc_cc_lib(
    name = "buffer_tools",
    hdrs = ["buffer_tools.h"],
    deps = [
        "//third_party/absl/base:core_headers",
    ],
)

sc_cc_lib(
    name = "managed_attribute",
    hdrs = ["managed_attribute.h"],
    deps = [
        ":attribute_database_interface",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/lib:macros",
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
        "//third_party/stratum/glue/status",
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
        ":google_switch_configurator",
        ":managed_attribute",
        ":phal_proto",
        ":system_interface",
        ":threadpool_interface",
        ":udev_event_handler",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/channel",
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
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
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
        ":dummy_threadpool",
        ":managed_attribute",
        ":test_util",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/absl/base:core_headers",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/hal/lib/phal/test:test_proto",
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
        "//third_party/absl/memory",
    ],
)

sc_cc_lib(
    name = "datasource",
    srcs = ["datasource.cc"],
    hdrs = ["datasource.h"],
    deps = [
        ":attribute_database_interface",
        ":managed_attribute",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/absl/time",
        "//third_party/stratum/glue/status",
    ],
)

sc_cc_lib(
    name = "datasource_mock",
    testonly = 1,
    hdrs = ["datasource_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":datasource",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/glue/status",
    ],
)

sc_proto_lib(
    name = "db_proto",
    hdrs = ["db.proto"],
    python_support = True,
    deps = [
        "//third_party/stratum/public/proto:hal_proto",
    ],
)

sc_cc_lib(
    name = "dummy_threadpool",
    srcs = ["dummy_threadpool.cc"],
    hdrs = ["dummy_threadpool.h"],
    deps = [
        ":threadpool_interface",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status",
    ],
)

sc_cc_lib(
    name = "filepath_stringsource",
    hdrs = ["filepath_stringsource.h"],
    deps = [
        ":stringsource_interface",
        ":system_interface",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:utils",
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
        "//third_party/absl/memory",
        "//third_party/stratum/lib:macros",
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
        "//third_party/absl/memory",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/hal/lib/phal/test:test_proto",
    ],
)

sc_cc_lib(
    name = "fixed_stringsource",
    hdrs = ["fixed_stringsource.h"],
    deps = [
        ":stringsource_interface",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:statusor",
    ],
)

sc_cc_lib(
    name = "google_eeprom_datasource",
    srcs = ["google_eeprom_datasource.cc"],
    hdrs = ["google_eeprom_datasource.h"],
    deps = [
        ":buffer_tools",
        ":datasource",
        ":managed_attribute",
        ":stringsource_interface",
        "//third_party/absl/memory",
        "//third_party/absl/time",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/lib:macros",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "google_eeprom_datasource_test",
    srcs = ["google_eeprom_datasource_test.cc"],
    deps = [
        ":fixed_stringsource",
        ":google_eeprom_datasource",
        ":test_util",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/absl/memory",
        "//third_party/absl/time",
        "//third_party/stratum/glue/status:status_test_util",
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
        "//third_party/absl/base:core_headers",
        "//third_party/absl/synchronization",
        "//third_party/absl/time",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/common:phal_interface",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/public/proto:hal_grpc_proto",
    ],
)

sc_cc_test(
    name = "google_phal_test",
    srcs = ["google_phal_test.cc"],
    deps = [
        ":attribute_database_mock",
        ":google_phal",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/absl/time",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib/channel:channel_mock",
    ],
)

sc_cc_lib(
    name = "google_switch_configurator",
    srcs = ["google_switch_configurator.cc"],
    hdrs = ["google_switch_configurator.h"],
    deps = [
        ":attribute_group",
        ":datasource",
        ":filepath_stringsource",
        ":google_eeprom_datasource",
        ":phal_proto",
        ":reader_writer_datasource",
        ":system_interface",
        ":transceiver_datasources",
        ":udev_event_handler",
        "//third_party/absl/memory",
        "//third_party/absl/time",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
    ],
)

sc_cc_test(
    name = "google_switch_configurator_test",
    srcs = ["google_switch_configurator_test.cc"],
    deps = [
        ":attribute_group_mock",
        ":db_proto",
        ":fixed_stringsource",
        ":google_switch_configurator",
        ":system_fake",
        ":test_util",
        ":udev_event_handler_mock",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
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
        "//third_party/absl/strings",
        "//third_party/stratum/glue:init_google",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "reader_writer_datasource",
    hdrs = ["reader_writer_datasource.h"],
    deps = [
        ":datasource",
        ":stringsource_interface",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
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
        "//third_party/absl/memory",
        "//third_party/stratum/glue/status:status_test_util",
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
        "//third_party/absl/memory",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/lib:macros",
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
        "//third_party/absl/memory",
        "//third_party/stratum/glue/status:status_test_util",
    ],
)

sc_cc_lib(
    name = "stringsource_interface",
    hdrs = ["stringsource_interface.h"],
    deps = [
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
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
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "system_interface",
    hdrs = ["system_interface.h"],
    deps = [
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
    ],
)

sc_cc_lib(
    name = "system_real",
    srcs = ["system_real.cc"],
    hdrs = ["system_real.h"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":system_interface",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/prebuilt/eudev",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
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
        "//third_party/stratum/glue/status:status_test_util",
    ],
)

sc_cc_lib(
    name = "threadpool_interface",
    hdrs = ["threadpool_interface.h"],
    deps = [
        "//third_party/absl/base:core_headers",
        "//third_party/stratum/glue/status",
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
    name = "transceiver_datasources",
    srcs = ["transceiver_datasources.cc"],
    hdrs = ["transceiver_datasources.h"],
    deps = [
        ":fixed_layout_datasource",
        ":managed_attribute",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/time",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/public/proto:hal_proto",
    ],
)

sc_cc_test(
    name = "transceiver_datasources_test",
    srcs = ["transceiver_datasources_test.cc"],
    deps = [
        ":fixed_stringsource",
        ":test_util",
        ":transceiver_datasources",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/absl/time",
        "//third_party/stratum/glue/status:status_test_util",
    ],
)

sc_cc_lib(
    name = "udev_event_handler",
    srcs = ["udev_event_handler.cc"],
    hdrs = ["udev_event_handler.h"],
    deps = [
        ":system_interface",
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/lib:macros",
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
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "legacy_phal",
    srcs = ["legacy_phal.cc"],
    hdrs = ["legacy_phal.h"],
    deps = [
        ":udev_interface",
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/common:phal_interface",
        "//third_party/stratum/hal/lib/phal:phal_proto",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//util/gtl:stl_util",
    ],
)

sc_proto_lib(
    name = "phal_proto",
    hdrs = ["phal.proto"],
    python_support = True,
    deps = [
        "//third_party/stratum/public/proto:hal_proto",
    ],
)

sc_cc_lib(
    name = "phal_proxy",
    srcs = ["phal_proxy.cc"],
    hdrs = ["phal_proxy.h"],
    deps = [
        ":phal_proto",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/prebuilt/grpc",
        "//third_party/stratum/hal/lib/common:client_sync_reader_writer",
        "//third_party/stratum/hal/lib/common:common_proto",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/common:phal_interface",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib/sandcastle:hardware_status_service_grpc_proto",
        "//third_party/stratum/public/proto:error_proto",
    ],
)

sc_cc_lib(
    name = "phal_sim",
    srcs = ["phal_sim.cc"],
    hdrs = ["phal_sim.h"],
    deps = [
        ":phal_proto",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/synchronization",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/common:phal_interface",
        "//third_party/stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "udev",
    srcs = ["udev.cc"],
    hdrs = ["udev.h"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":udev_interface",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/prebuilt/eudev",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "udev_interface",
    hdrs = ["udev_interface.h"],
    deps = [
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
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

sc_cc_lib(
    name = "led_datasource",
    srcs = ["led_datasource.cc"],
    hdrs = ["led_datasource.h"],
    deps = [
        ":datasource",
        ":filepath_stringsource",
        ":phal_proto",
        ":reader_writer_datasource",
        ":stringsource_interface",
        ":system_interface",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/prebuilt/eudev",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/public/proto:hal_proto",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "led_datasource_test",
    srcs = ["led_datasource_test.cc"],
    deps = [
        ":led_datasource",
        ":phal_proto",
        ":system_interface_mock",
        "//testing/base/public:gunit_main_no_google3",
        "//third_party/absl/memory",
        "//third_party/protobuf",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/public/proto:hal_proto",
    ],
)
