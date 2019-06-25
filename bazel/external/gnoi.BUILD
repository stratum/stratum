load(
    "@com_github_stratum_stratum//bazel/rules:proto_rule.bzl",
    "wrapped_proto_library",
)
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

NEW_PROTO_DIR = "gnoi/"
IMPORT_REWRITE_MAP = {
    "github.com/openconfig/gnoi/": "gnoi/"
}

package(
    default_visibility = [ "//visibility:public" ],
)

wrapped_proto_library(
    name = "types_proto",
    srcs = ["types/types.proto"],
    deps = [
        "@com_google_protobuf//:descriptor_proto",
        "@com_google_protobuf//:any_proto",
    ],
    new_proto_dir = NEW_PROTO_DIR,
    rewrite_proto_imports = IMPORT_REWRITE_MAP,
)

cc_proto_library(
    name = "types_cc_proto",
    deps = [":types_proto"]
)

wrapped_proto_library(
    name = "common_proto",
    srcs = ["common/common.proto"],
    deps = [":types_proto"],
    new_proto_dir = NEW_PROTO_DIR,
    rewrite_proto_imports = IMPORT_REWRITE_MAP,
)

cc_proto_library(
    name = "common_cc_proto",
    deps = [":common_proto"],
)

wrapped_proto_library(
    name = "diag_proto",
    srcs = ["diag/diag.proto"],
    deps = [":types_proto"],
    new_proto_dir = NEW_PROTO_DIR,
    rewrite_proto_imports = IMPORT_REWRITE_MAP,
)

cc_proto_library(
    name = "diag_cc_proto",
    deps = [":diag_proto"]
)

cc_grpc_library(
    name = "diag_cc_grpc",
    srcs = [":diag_proto"],
    deps = [":diag_cc_proto"],
    grpc_only = True,
)

wrapped_proto_library(
    name = "system_proto",
    srcs = ["system/system.proto"],
    deps = [
      ":types_proto",
      ":common_proto"
    ],
    new_proto_dir = NEW_PROTO_DIR,
    rewrite_proto_imports = IMPORT_REWRITE_MAP,
)

cc_proto_library(
    name = "system_cc_proto",
    deps = [":system_proto"]
)

cc_grpc_library(
    name = "system_cc_grpc",
    srcs = [":system_proto"],
    deps = [":system_cc_proto"],
    grpc_only = True,
)

wrapped_proto_library(
    name = "file_proto",
    srcs = ["file/file.proto"],
    deps = [
      ":types_proto",
      ":common_proto"
    ],
    new_proto_dir = NEW_PROTO_DIR,
    rewrite_proto_imports = IMPORT_REWRITE_MAP,
)

cc_proto_library(
    name = "file_cc_proto",
    deps = [":file_proto"],
)

cc_grpc_library(
    name = "file_cc_grpc",
    srcs = [":file_proto"],
    deps = [":file_cc_proto"],
    grpc_only = True,
)

wrapped_proto_library(
    name = "cert_proto",
    srcs = ["cert/cert.proto"],
    deps = [
      ":types_proto",
      ":common_proto",
    ],
    new_proto_dir = NEW_PROTO_DIR,
    rewrite_proto_imports = IMPORT_REWRITE_MAP,
)

cc_proto_library(
    name = "cert_cc_proto",
    deps = [":cert_proto"],
)

cc_grpc_library(
    name = "cert_cc_grpc",
    srcs = [":cert_proto"],
    deps = [":cert_cc_proto"],
    grpc_only = True,
)
