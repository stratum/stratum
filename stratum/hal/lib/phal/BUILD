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
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_cc_proto_library",
    "EMBEDDED_ARCHES",
    "HOST_ARCHES",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "attribute_database_interface",
    hdrs = ["attribute_database_interface.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/types:variant",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/phal:db_proto",
        "//stratum/lib/channel",
    ],
)

stratum_cc_library(
    name = "attribute_database_mock",
    testonly = 1,
    hdrs = ["attribute_database_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_database_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "buffer_tools",
    hdrs = ["buffer_tools.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_library(
    name = "managed_attribute",
    hdrs = ["managed_attribute.h"],
    deps = [
        ":attribute_database_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "managed_attribute_mock",
    testonly = 1,
    hdrs = ["managed_attribute_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_database_interface",
        ":datasource",
        ":managed_attribute",
        "@com_google_googletest//:gtest",
        "//stratum/glue/status",
    ],
)

stratum_cc_library(
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
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
    ],
)

stratum_cc_library(
    name = "attribute_group",
    srcs = ["attribute_group.cc"],
    hdrs = ["attribute_group.h"],
    deps = [
        ":attribute_database_interface",
        ":datasource",
        ":managed_attribute",
        ":threadpool_interface",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//util/gtl:flat_hash_map",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "attribute_group_test",
    srcs = ["attribute_group_test.cc"],
    deps = [
        ":attribute_group",
        ":datasource",
        ":dummy_threadpool",
        ":managed_attribute",
        ":test_util",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/phal/test:test_proto",
    ],
)

stratum_cc_library(
    name = "attribute_group_mock",
    testonly = 1,
    srcs = ["attribute_group_mock.cc"],
    hdrs = ["attribute_group_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_group",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
    ],
)

stratum_cc_library(
    name = "datasource",
    srcs = ["datasource.cc"],
    hdrs = ["datasource.h"],
    deps = [
        ":attribute_database_interface",
        ":managed_attribute",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
    ],
)

stratum_cc_library(
    name = "datasource_mock",
    testonly = 1,
    hdrs = ["datasource_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":datasource",
        "@com_google_googletest//:gtest",
        "//stratum/glue/status",
    ],
)

stratum_cc_proto_library(
    name = "db_proto",
    hdrs = ["db.proto"],
    #python_support = True,
    deps = [
        "//stratum/public/proto:hal_proto",
    ],
)

stratum_cc_library(
    name = "dummy_threadpool",
    srcs = ["dummy_threadpool.cc"],
    hdrs = ["dummy_threadpool.h"],
    deps = [
        ":threadpool_interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
    ],
)

stratum_cc_library(
    name = "filepath_stringsource",
    hdrs = ["filepath_stringsource.h"],
    deps = [
        ":stringsource_interface",
        ":system_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:utils",
    ],
)

stratum_cc_library(
    name = "fixed_layout_datasource",
    srcs = ["fixed_layout_datasource.cc"],
    hdrs = ["fixed_layout_datasource.h"],
    deps = [
        ":buffer_tools",
        ":datasource",
        ":managed_attribute",
        ":stringsource_interface",
        "@com_google_absl//absl/memory",
        "//stratum/lib:macros",
    ],
)

stratum_cc_test(
    name = "fixed_layout_datasource_test",
    srcs = ["fixed_layout_datasource_test.cc"],
    deps = [
        ":fixed_layout_datasource",
        ":fixed_stringsource",
        ":test_util",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/phal/test:test_proto",
    ],
)

stratum_cc_library(
    name = "fixed_stringsource",
    hdrs = ["fixed_stringsource.h"],
    deps = [
        ":stringsource_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "google_eeprom_datasource",
    srcs = ["google_eeprom_datasource.cc"],
    hdrs = ["google_eeprom_datasource.h"],
    deps = [
        ":buffer_tools",
        ":datasource",
        ":managed_attribute",
        ":stringsource_interface",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "google_eeprom_datasource_test",
    srcs = ["google_eeprom_datasource_test.cc"],
    deps = [
        ":fixed_stringsource",
        ":google_eeprom_datasource",
        ":test_util",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "//stratum/glue/status:status_test_util",
    ],
)

stratum_cc_library(
    name = "google_phal",
    srcs = ["google_phal.cc"],
    hdrs = ["google_phal.h"],
    deps = [
        ":attribute_database",
        ":attribute_database_interface",
        ":system_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
        "//stratum/public/proto:hal_proto",
    ],
)

stratum_cc_test(
    name = "google_phal_test",
    srcs = ["google_phal_test.cc"],
    deps = [
        ":attribute_database_mock",
        ":google_phal",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/channel:channel_mock",
    ],
)

stratum_cc_library(
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
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
    ],
)

stratum_cc_test(
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
        "@com_google_googletest//:gtest_main",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
    ],
)

stratum_cc_binary(
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
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "//stratum/glue:init_google",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
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
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/glue/status:status_test_util",
    ],
)

stratum_cc_library(
    name = "regex_datasource",
    srcs = ["regex_datasource.cc"],
    hdrs = ["regex_datasource.h"],
    deps = [
        ":datasource",
        ":managed_attribute",
        ":stringsource_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "@com_googlesource_code_re2//:re2",
    ],
)

stratum_cc_test(
    name = "regex_datasource_test",
    srcs = ["regex_datasource_test.cc"],
    deps = [
        ":datasource",
        ":fixed_stringsource",
        ":regex_datasource",
        ":test_util",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/glue/status:status_test_util",
    ],
)

stratum_cc_library(
    name = "stringsource_interface",
    hdrs = ["stringsource_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "system_fake",
    testonly = 1,
    srcs = ["system_fake.cc"],
    hdrs = ["system_fake.h"],
    arches = HOST_ARCHES,
    deps = [
        ":system_interface",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "system_interface",
    hdrs = ["system_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "system_real",
    srcs = ["system_real.cc"],
    hdrs = ["system_real.h"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":system_interface",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//sandblaze/prebuilt/eudev",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
    ],
)

stratum_cc_library(
    name = "system_interface_mock",
    testonly = 1,
    hdrs = ["system_interface_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":system_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "test_util",
    testonly = 1,
    hdrs = ["test_util.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_database_interface",
        ":datasource",
        ":managed_attribute",
        "@com_google_googletest//:gtest",
        "//stratum/glue/status:status_test_util",
    ],
)

stratum_cc_library(
    name = "threadpool_interface",
    hdrs = ["threadpool_interface.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "//stratum/glue/status",
    ],
)

stratum_cc_library(
    name = "threadpool_interface_mock",
    testonly = 1,
    hdrs = ["threadpool_interface_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":threadpool_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "transceiver_datasources",
    srcs = ["transceiver_datasources.cc"],
    hdrs = ["transceiver_datasources.h"],
    deps = [
        ":fixed_layout_datasource",
        ":managed_attribute",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "//stratum/lib:macros",
        "//stratum/public/proto:hal_proto",
    ],
)

stratum_cc_test(
    name = "transceiver_datasources_test",
    srcs = ["transceiver_datasources_test.cc"],
    deps = [
        ":fixed_stringsource",
        ":test_util",
        ":transceiver_datasources",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/time",
        "//stratum/glue/status:status_test_util",
    ],
)

stratum_cc_library(
    name = "udev_event_handler",
    srcs = ["udev_event_handler.cc"],
    hdrs = ["udev_event_handler.h"],
    deps = [
        ":system_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "udev_event_handler_mock",
    testonly = 1,
    hdrs = ["udev_event_handler_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":udev_event_handler",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_test(
    name = "udev_event_handler_test",
    srcs = ["udev_event_handler_test.cc"],
    deps = [
        ":system_fake",
        ":udev_event_handler",
        ":udev_event_handler_mock",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "legacy_phal",
    srcs = ["legacy_phal.cc"],
    hdrs = ["legacy_phal.h"],
    deps = [
        ":udev_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:phal_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/glue/gtl:stl_util",
    ],
)

stratum_cc_proto_library(
    name = "phal_proto",
    hdrs = ["phal.proto"],
    #python_support = True,
    deps = [
        "//stratum/public/proto:hal_proto",
    ],
)

stratum_cc_library(
    name = "phal_proxy",
    srcs = ["phal_proxy.cc"],
    hdrs = ["phal_proxy.h"],
    deps = [
        ":phal_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_grpc//:grpc++",
        "//stratum/hal/lib/common:client_sync_reader_writer",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib/sandcastle:hardware_status_service_proto",
        "//stratum/public/proto:error_proto",
    ],
)

stratum_cc_library(
    name = "phal_sim",
    srcs = ["phal_sim.cc"],
    hdrs = ["phal_sim.h"],
    deps = [
        ":phal_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "udev",
    srcs = ["udev.cc"],
    hdrs = ["udev.h"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":udev_interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//sandblaze/prebuilt/eudev",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "udev_interface",
    hdrs = ["udev_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "udev_mock",
    testonly = 1,
    hdrs = ["udev_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":udev_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
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
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//sandblaze/prebuilt/eudev",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/public/proto:hal_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "led_datasource_test",
    srcs = ["led_datasource_test.cc"],
    deps = [
        ":led_datasource",
        ":phal_proto",
        ":system_interface_mock",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue/status:status_test_util",
        "//stratum/public/proto:hal_proto",
    ],
)
