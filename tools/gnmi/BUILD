# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

cc_binary(
    name = "gnmi-cli",
    srcs = ["gnmi_cli.cc"],
    deps = [
        "@com_github_gflags_gflags//:gflags",
        "@com_google_protobuf//:protobuf",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_proto",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
    ]
)
