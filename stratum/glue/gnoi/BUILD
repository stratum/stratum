load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "SANDBLAZE_INTERNAL",
    "sc_proto_lib",
)

package(default_visibility = SANDBLAZE_INTERNAL)

licenses(["notice"])  # Apache v2

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

sc_proto_lib(
    name = "cert_proto",
    hdrs = ["cert.proto"],
    services = ["grpc"],
    deps = [
        ":types_proto",
    ],
)

sc_proto_lib(
    name = "diag_proto",
    hdrs = ["diag.proto"],
    services = ["grpc"],
    deps = [
        ":types_proto",
    ],
)

sc_proto_lib(
    name = "file_proto",
    hdrs = ["file.proto"],
    services = ["grpc"],
    deps = [
        ":types_proto",
    ],
)

sc_proto_lib(
    name = "interface_proto",
    hdrs = ["interface.proto"],
    services = ["grpc"],
    deps = [
        ":types_proto",
    ],
)

sc_proto_lib(
    name = "system_proto",
    hdrs = ["system.proto"],
    services = ["grpc"],
    deps = [
        ":types_proto",
    ],
)

sc_proto_lib(
    name = "types_proto",
    hdrs = ["types.proto"],
)
