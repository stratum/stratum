# Copyright 2020-present Open Networking Foundation
# Copyright 2020 PLVision
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "tai_interface",
    hdrs = ["tai_interface.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "taish_client",
    srcs = ["taish_client.cc"],
    hdrs = ["taish_client.h"],
    deps = [
        ":tai_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_telecominfraproject_oopt_tai_taish//:taish_cc_grpc",
        "@com_github_telecominfraproject_oopt_tai_taish//:taish_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "tai_optics_datasource",
    srcs = ["tai_optics_datasource.cc"],
    hdrs = ["tai_optics_datasource.h"],
    deps = [
        ":tai_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "tai_phal",
    srcs = ["tai_phal.cc"],
    hdrs = ["tai_phal.h"],
    deps = [
        ":tai_interface",
        ":tai_switch_configurator",
        ":taish_client",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:optics_adapter",
        "//stratum/hal/lib/phal:phal_backend_interface",
        "//stratum/lib:macros",
        "//stratum/lib/channel",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "tai_switch_configurator",
    srcs = ["tai_switch_configurator.cc"],
    hdrs = ["tai_switch_configurator.h"],
    deps = [
        ":tai_optics_datasource",
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_group",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "tai_interface_mock",
    hdrs = ["tai_interface_mock.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_test(
    name = "tai_optics_datasource_test",
    srcs = ["tai_optics_datasource_test.cc"],
    deps = [
        ":tai_interface",
        ":tai_interface_mock",
        ":tai_optics_datasource",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_test(
    name = "tai_switch_configurator_test",
    srcs = ["tai_switch_configurator_test.cc"],
    deps = [
        ":tai_interface",
        ":tai_interface_mock",
        ":tai_switch_configurator",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_group_mock",
        "//stratum/hal/lib/phal:db_cc_proto",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_test(
    name = "tai_phal_test",
    srcs = ["tai_phal_test.cc"],
    deps = [
        ":tai_phal",
        ":tai_interface",
        ":tai_interface_mock",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ]
)
