load(
    "//bazel:rules.bzl",
    "SANDBLAZE_INTERNAL",
    "stratum_cc_proto_library",
)

package(default_visibility = SANDBLAZE_INTERNAL)

# This command edits the gnoi micro service protos to be able to be used by Hercules stack.
proto_edit_cmd = ("cat $(SRCS) " +
                  "| sed '/^syntax.*/a option cc_generic_services = false;' " +
                  "| sed 's|openconfig/gnoi/|platforms/networking/hercules/glue/gnoi/|g' " +
                  "| sed 's|net/proto2/proto|google/protobuf|g' " +
                  "| sed 's|proto2.|google.protobuf.|g' > $@")

genrule(
    name = "cert_proto_srcs",
    srcs = [
        "//openconfig/gnoi/cert:cert_proto_srcs",
    ],
    outs = ["cert.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "diag_proto_srcs",
    srcs = [
        "//openconfig/gnoi/diag:diag_proto_srcs",
    ],
    outs = ["diag.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "file_proto_srcs",
    srcs = [
        "//openconfig/gnoi/file:file_proto_srcs",
    ],
    outs = ["file.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "interface_proto_srcs",
    srcs = [
        "//openconfig/gnoi/interface:interface_proto_srcs",
    ],
    outs = ["interface.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "system_proto_srcs",
    srcs = [
        "//openconfig/gnoi/system:system_proto_srcs",
    ],
    outs = ["system.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "types_proto_srcs",
    srcs = [
        "//openconfig/gnoi:types_proto_srcs",
    ],
    outs = ["types.proto"],
    cmd = "cp $(SRCS) $(@D)",
    visibility = ["//visibility:private"],
)

stratum_cc_proto_library(
    name = "cert_proto",
    hdrs = ["cert.proto"],
    with_grpc = True,
    deps = [
        ":types_proto",
    ],
)

stratum_cc_proto_library(
    name = "diag_proto",
    hdrs = ["diag.proto"],
    with_grpc = True,
    deps = [
        ":types_proto",
    ],
)

stratum_cc_proto_library(
    name = "file_proto",
    hdrs = ["file.proto"],
    with_grpc = True,
    deps = [
        ":types_proto",
    ],
)

stratum_cc_proto_library(
    name = "interface_proto",
    hdrs = ["interface.proto"],
    with_grpc = True,
    deps = [
        ":types_proto",
    ],
)

stratum_cc_proto_library(
    name = "system_proto",
    hdrs = ["system.proto"],
    with_grpc = True,
    deps = [
        ":types_proto",
    ],
)

stratum_cc_proto_library(
    name = "types_proto",
    hdrs = ["types.proto"],
)
