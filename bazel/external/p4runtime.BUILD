load(
    "@com_github_stratum_stratum//bazel/rules:proto_rule.bzl",
    "stratum_cc_proto_library",
    "wrapped_cc_proto_library",
)

SOURCES_ROOT = "proto"

package(
    default_visibility = [ "//visibility:public" ],
)

''' FIXME
Required targets:
p4info_proto
p4runtime_proto
p4_device_config_proto
util
code_proto

Note: separate targets for gRPC and non-gRPC
'''

wrapped_cc_proto_library(
    name = "p4types_proto",
    proto_source_root = SOURCES_ROOT,
    srcs = [
        "proto/p4/config/v1/p4types.proto",
    ],
    with_grpc = False,
    include_wkt = False,
)

wrapped_cc_proto_library(
    name = "p4config_proto",
    proto_source_root = SOURCES_ROOT,
    srcs = [
        "proto/p4/tmp/p4config.proto",
    ],
    with_grpc = False,
    include_wkt = False,
)

wrapped_cc_proto_library(
    name = "p4info_proto",
    proto_source_root = SOURCES_ROOT,
    srcs = ["proto/p4/config/v1/p4info.proto"],
    deps = [":p4types_proto"],
    with_grpc = False,
    include_wkt = True,
)

wrapped_cc_proto_library(
    name = "p4runtime_proto",
    proto_source_root = SOURCES_ROOT,
    srcs = [
        "proto/p4/v1/p4runtime.proto",
    ],
    deps = [
        ":p4types_proto",
        ":p4info_proto",
        ":p4data_proto",
        "@com_github_googleapis//:status_proto",
    ],
    with_grpc = True,
    include_wkt = True,
)

wrapped_cc_proto_library(
    name = "p4data_proto",
    proto_source_root = SOURCES_ROOT,
    srcs = ["proto/p4/v1/p4data.proto"],
    deps = [],
    with_grpc = False,
    include_wkt = False,
)
