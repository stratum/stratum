load(
    "@com_github_stratum_stratum//bazel/rules:proto_rule.bzl",
    "wrapped_cc_proto_library",
)

PREFIX = "github.com/openconfig/gnoi/"

package(
    default_visibility = [ "//visibility:public" ],
)

'''FIXME require non-gRPC and gRPC rules'''

wrapped_cc_proto_library(
  name = "gnoi_cc_proto",
  prefix = PREFIX,
  srcs = [
      "types.proto",
      "common.proto"
  ],
)

wrapped_cc_proto_library(
    name = "diag_cc_grpc",
    prefix = PREFIX,
    srcs = ["diag/diag.proto"],
    deps = [":gnoi_cc_proto"],
    with_grpc = True,
    include_wkt = False,
)

wrapped_cc_proto_library(
    name = "system_cc_grpc",
    prefix = PREFIX,
    srcs = ["system/system.proto"],
    deps = [":gnoi_cc_proto"],
    with_grpc = True,
    include_wkt = False,
)

wrapped_cc_proto_library(
    name = "file_cc_grpc",
    prefix = PREFIX,
    srcs = ["file/file.proto"],
    deps = [":gnoi_cc_proto"],
    with_grpc = True,
    include_wkt = False,
)

wrapped_cc_proto_library(
    name = "cert_cc_grpc",
    prefix = PREFIX,
    srcs = ["cert/cert.proto"],
    deps = [":gnoi_cc_proto"],
    with_grpc = True,
    include_wkt = False,
)