load(
    "@com_github_stratum_stratum//bazel/rules:proto_rule.bzl",
    "stratum_cc_proto_library",
    "wrapped_cc_proto_library",
)

PREFIX = "github.com/p4lang/PI/p4"
OLD_PREFIX = "p4"
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
    prefix = PREFIX,
    old_prefix = OLD_PREFIX,
    proto_source_root = SOURCES_ROOT,
    srcs = [
        "proto/p4/config/v1/p4types.proto",
    ],
    with_grpc = False,
    include_wkt = False,
    rewrite_proto_imports = True,
)

wrapped_cc_proto_library(
    name = "p4config_proto",
    prefix = PREFIX,
    old_prefix = OLD_PREFIX,
    proto_source_root = SOURCES_ROOT,
    srcs = [
        "proto/p4/tmp/p4config.proto",
    ],
    with_grpc = False,
    include_wkt = False,
    rewrite_proto_imports = True,
)

wrapped_cc_proto_library(
    name = "p4info_proto",
    prefix = PREFIX,
    old_prefix = OLD_PREFIX,
    proto_source_root = SOURCES_ROOT,
    srcs = ["proto/p4/config/v1/p4info.proto"],
    deps = [":p4types_proto"],
    with_grpc = False,
    include_wkt = True,
    rewrite_proto_imports = True,
)

wrapped_cc_proto_library(
    name = "p4runtime_proto",
    prefix = PREFIX,
    old_prefix = OLD_PREFIX,
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
    rewrite_proto_imports = True,
)

wrapped_cc_proto_library(
    name = "p4data_proto",
    prefix = PREFIX,
    old_prefix = OLD_PREFIX,
    proto_source_root = SOURCES_ROOT,
    srcs = ["proto/p4/v1/p4data.proto"],
    deps = [],
    with_grpc = False,
    include_wkt = False,
    rewrite_proto_imports = True,
)

genrule(
    name = "util_h",
    srcs = ["proto/PI/proto/util.h"],
    cmd = "sed 's#<p4/config/v1/p4info.pb.h>#\"github.com/p4lang/PI/p4/config/v1/p4info.pb.h\"#g' $< > $(OUTS)",
    outs = ["PI/proto/util.h"],
    visibility = [],
)

genrule(
    name = "util_cpp",
    srcs = ["proto/src/util.cpp"],
    cmd = "sed 's#<PI/proto/util.h>#\"PI/proto/util.h\"#g' $< > $(OUTS)",
    outs = ["src/util.cpp"],
    visibility = [],
)

cc_library(
    name = "util",
    hdrs = ["util_h"],
    srcs = ["util_cpp"],
    deps = [
        "p4info_proto",
    ],
)