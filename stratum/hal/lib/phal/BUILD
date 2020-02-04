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

licenses(["notice"])  # Apache v2

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_package",
    "EMBEDDED_ARCHES",
    "HOST_ARCHES",
)

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "attribute_database_interface",
    hdrs = ["attribute_database_interface.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/types:variant",
        "//stratum/glue:integral_types",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/phal:db_cc_proto",
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
        "@com_google_absl//absl/container:flat_hash_map",
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
        ":db_cc_proto",
        ":dummy_threadpool",
        ":managed_attribute",
        ":phal_cc_proto",
        ":phaldb_service",
        ":system_interface",
        ":threadpool_interface",
        ":udev_event_handler",
        ":switch_configurator_interface",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
    ],
)

stratum_cc_test(
    name = "attribute_database_test",
    srcs = ["attribute_database_test.cc"],
    data = [
        #FIXME(boc) google only"//stratum/hal/config:legacy_phal_init_config_generic_trident2_hercules.pb.txt",
    ],
    deps = [
        "system_fake",
        ":attribute_database",
        ":attribute_group_mock",
        ":db_cc_proto",
        ":dummy_threadpool",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
    ],
)

stratum_cc_library(
    name = "adapter",
    srcs = ["adapter.cc"],
    hdrs = ["adapter.h"],
    deps = [
        ":attribute_database_interface",
        ":db_cc_proto",
        "//stratum/glue/status",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
    ],
)

stratum_cc_library(
    name = "sfp_adapter",
    srcs = ["sfp_adapter.cc"],
    hdrs = ["sfp_adapter.h"],
    deps = [
        ":adapter",
        ":attribute_database_interface",
        ":db_cc_proto",
        ":managed_attribute",
        "@com_google_protobuf//:protobuf",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:utils",
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
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_test(
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
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/phal/test:test_cc_proto",
        "//stratum/lib/test_utils:matchers",
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
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

stratum_cc_library(
    name = "datasource",
    srcs = ["datasource.cc"],
    hdrs = ["datasource.h"],
    deps = [
        ":attribute_database_interface",
        ":managed_attribute",
        ":phal_cc_proto",
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
    srcs = ["datasource_mock.cc"],
    hdrs = ["datasource_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":datasource",
        "@com_google_googletest//:gtest",
        "//stratum/glue/status",
    ],
)

proto_library(
    name = "db_proto",
    srcs = ["db.proto"],
    deps = [
        "//stratum/hal/lib/common:common_proto",
    ],
)

cc_proto_library(
    name = "db_cc_proto",
    deps = [":db_proto"]
)

cc_grpc_library(
    name = "db_cc_grpc",
    srcs = [":db_proto"],
    deps = [":db_cc_proto"],
    grpc_only = True,
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
        "//stratum/lib:macros",
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
    copts = ["-funsigned-char"],
    deps = [
        ":fixed_layout_datasource",
        ":fixed_stringsource",
        ":test_util",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/phal/test:test_cc_proto",
        "//stratum/lib/test_utils:matchers",
    ],
)

stratum_cc_library(
    name = "fixed_stringsource",
    hdrs = ["fixed_stringsource.h"],
    deps = [
        ":stringsource_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "phaldb_service",
    srcs = ["phaldb_service.cc"],
    hdrs = ["phaldb_service.h"],
    deps = [
        ":adapter",
        ":attribute_database_interface",
        ":db_cc_grpc",
        ":db_cc_proto",
        ":managed_attribute",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:cleanup",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:channel_writer_wrapper",
        "//stratum/hal/lib/common:error_buffer",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_binary(
    name = "phal_cli",
    srcs = ["phal_cli.cc"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":db_cc_grpc",
        ":db_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/glue/status",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
    ],
)

stratum_cc_test(
    name = "phaldb_service_test",
    srcs = ["phaldb_service_test.cc"],
    deps = [
        ":attribute_database_mock",
        ":phaldb_service",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_googletest//:gtest",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

''' FIXME(boc) google only
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
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
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
        "//stratum/lib/test_utils:matchers",
    ],
)
'''

stratum_package(
    name = "phal_cli_pkg",
    arches = EMBEDDED_ARCHES,
    bins = [":phal_cli"],
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
        "//stratum/lib/test_utils:matchers",
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
        "//stratum/lib/test_utils:matchers",
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
        "@com_github_systemd_systemd//:libudev",
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
        "//stratum/lib/test_utils:matchers",
    ],
)

stratum_cc_library(
    name = "threadpool_interface",
    hdrs = ["threadpool_interface.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "//stratum/glue/status",
        "//stratum/glue:integral_types",
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
    name = "udev_event_handler",
    srcs = ["udev_event_handler.cc"],
    hdrs = ["udev_event_handler.h"],
    deps = [
        ":system_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
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
        "//stratum/lib/test_utils:matchers",
    ],
)

''' FIXME(boc) google only
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
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/glue/gtl:stl_util",
    ],
)
'''

proto_library(
    name = "phal_proto",
    srcs = ["phal.proto"],
    deps = [
        "//stratum/hal/lib/common:common_proto",
    ],
)

cc_proto_library(
    name = "phal_cc_proto",
    deps = [":phal_proto"]
)

'''FIXME(boc) google only
stratum_cc_library(
    name = "phal_proxy",
    srcs = ["phal_proxy.cc"],
    hdrs = ["phal_proxy.h"],
    deps = [
        ":phal_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/hal/lib/common:client_sync_reader_writer",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib/sandcastle:hardware_status_service_grpc_proto",
        "//stratum/public/proto:error_proto",
    ],
)
'''

stratum_cc_library(
    name = "phal_sim",
    srcs = ["phal_sim.cc"],
    hdrs = ["phal_sim.h"],
    deps = [
        ":phal_cc_proto",
        ":sfp_configurator",
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
        "@com_github_systemd_systemd//:libudev",
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
    name = "switch_configurator_interface",
    hdrs = ["switch_configurator_interface.h"],
    deps = [
        ":attribute_group",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/glue/status",
    ],
)

stratum_cc_library(
    name = "sfp_configurator",
    hdrs = ["sfp_configurator.h"],
    deps = [
        ":attribute_group",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/glue/status",
    ],
)

