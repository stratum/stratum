load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_proto_library",
)

package(default_visibility = STRATUM_INTERNAL)

licenses(["notice"])  # Apache v2

# This command edits the gnoi micro service protos to be able to be used by Hercules stack.
proto_edit_cmd = ("cat $(SRCS) " +
                  "| sed '/^syntax.*/a option cc_generic_services = false;' " +
                  "| sed 's|openconfig/gnmi/proto/gnmi_ext/|platforms/networking/hercules/glue/gnmi/|g' " +
                  "| sed 's|net/proto2/proto|google/protobuf|g' " +
                  "| sed 's|proto2.|google.protobuf.|g' > $@")

genrule(
    name = "gnmi_proto_srcs",
    srcs = [
        "//openconfig/gnmi/proto/gnmi:gnmi_proto_srcs",
    ],
    outs = ["gnmi.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "gnmi_ext_proto_srcs",
    srcs = [
        "//openconfig/gnmi/proto/gnmi_ext:gnmi_ext_proto_srcs",
    ],
    outs = ["gnmi_ext.proto"],
    cmd = "cp $(SRCS) $(@D)",
    visibility = ["//visibility:private"],
)

stratum_cc_proto_library(
    name = "gnmi_proto",
    hdrs = ["gnmi.proto"],
    with_grpc = True,
    deps = [
        ":gnmi_ext_proto",
    ],
)

stratum_cc_proto_library(
    name = "gnmi_ext_proto",
    hdrs = ["gnmi_ext.proto"],
)
