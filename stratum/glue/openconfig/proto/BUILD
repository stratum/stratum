load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
)

package(default_visibility = STRATUM_INTERNAL)

licenses(["notice"])  # Apache v2

# This command edits the old/deprecated openconfig protos to be compiled by
# cc_proto_library rule. This command is expected to go away as soon as we move
# to the new protos.
old_proto_edit_cmd = ("cat $(SRCS) " +
                      "| sed 's|platforms/networking/hercules/public/proto/openconfig/old/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                      "| sed 's|/ywrappers|/old_ywrapper|g' > $@")

# This command edits the third_party openconfig protos to be compiled by
# cc_proto_library rule.
proto_edit_cmd = ("cat $(SRCS) " +
                  "| sed 's|golang/ygot/proto/ywrapper/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                  "| sed 's|golang/ygot/proto/yext/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                  "| sed 's|openconfig/hercules/proto/openconfig/enums/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                  "| sed 's|openconfig/hercules/proto/openconfig/hercules/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                  "| sed 's|openconfig/hercules/proto/openconfig/openconfig_interfaces/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                  "| sed 's|openconfig/hercules/proto/openconfig/openconfig_lacp/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                  "| sed 's|openconfig/hercules/proto/openconfig/openconfig_platform/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                  "| sed 's|openconfig/hercules/proto/openconfig/openconfig_qos/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                  "| sed 's|openconfig/hercules/proto/openconfig/openconfig_system/|platforms/networking/hercules/glue/openconfig/proto/|g' " +
                  "| sed 's|net/proto2/proto|google/protobuf|g' " +
                  "| sed 's|proto2.|google.protobuf.|g' > $@")

# TODO(aghaffar): Remove the old proto genrules when we start using the new protos.
genrule(
    name = "old_yext_proto_srcs",
    srcs = [
        "//stratum/public/proto/openconfig/old:yext_proto_srcs",
    ],
    outs = ["old_yext.proto"],
    cmd = old_proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "old_ywrapper_proto_srcs",
    srcs = [
        "//stratum/public/proto/openconfig/old:ywrapper_proto_srcs",
    ],
    outs = ["old_ywrapper.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "old_openconfig_proto_srcs",
    srcs = [
        "//stratum/public/proto/openconfig/old:openconfig_proto_srcs",
    ],
    outs = ["old_openconfig.proto"],
    cmd = old_proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "old_openconfig_google_bcm_proto_srcs",
    srcs = [
        "//stratum/public/proto/openconfig/old:openconfig_google_bcm_proto_srcs",
    ],
    outs = ["old_openconfig_google_bcm.proto"],
    cmd = old_proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "yext_proto_srcs",
    srcs = [
        "//golang/ygot/proto/yext:yext_proto_srcs",
    ],
    outs = ["yext.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "ywrapper_proto_srcs",
    srcs = [
        "//golang/ygot/proto/ywrapper:ywrapper_proto_srcs",
    ],
    outs = ["ywrapper.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "enum_proto_srcs",
    srcs = [
        "//openconfig/hercules/proto/openconfig/enums:enums_proto_srcs",
    ],
    outs = ["enums.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "hercules_proto_srcs",
    srcs = [
        "//openconfig/hercules/proto/openconfig/hercules:hercules_proto_srcs",
    ],
    outs = ["hercules.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "openconfig_interfaces_proto_srcs",
    srcs = [
        "//openconfig/hercules/proto/openconfig/openconfig_interfaces:openconfig_interfaces_proto_srcs",
    ],
    outs = ["openconfig_interfaces.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "openconfig_lacp_proto_srcs",
    srcs = [
        "//openconfig/hercules/proto/openconfig/openconfig_lacp:openconfig_lacp_proto_srcs",
    ],
    outs = ["openconfig_lacp.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "openconfig_platform_proto_srcs",
    srcs = [
        "//openconfig/hercules/proto/openconfig/openconfig_platform:openconfig_platform_proto_srcs",
    ],
    outs = ["openconfig_platform.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "openconfig_qos_proto_srcs",
    srcs = [
        "//openconfig/hercules/proto/openconfig/openconfig_qos:openconfig_qos_proto_srcs",
    ],
    outs = ["openconfig_qos.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "openconfig_system_proto_srcs",
    srcs = [
        "//openconfig/hercules/proto/openconfig/openconfig_system:openconfig_system_proto_srcs",
    ],
    outs = ["openconfig_system.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

genrule(
    name = "openconfig_proto_srcs",
    srcs = [
        "//openconfig/hercules/proto/openconfig:openconfig_proto_srcs",
    ],
    outs = ["openconfig.proto"],
    cmd = proto_edit_cmd,
    visibility = ["//visibility:private"],
)

# TODO(aghaffar): Remove the old proto rule when we start using the new protos.
proto_library(
    name = "old_openconfig_proto",
    srcs = [
        "old_openconfig.proto",
        "old_openconfig_google_bcm.proto",
        "old_yext.proto",
        "old_ywrapper.proto",
    ],
)
cc_proto_library(
    name = "old_openconfig_cc_proto",
    deps = [":old_openconfig_proto"],
)

proto_library(
    name = "openconfig_proto",
    srcs = [
        "enums.proto",
        "hercules.proto",
        "openconfig.proto",
        "openconfig_interfaces.proto",
        "openconfig_lacp.proto",
        "openconfig_platform.proto",
        "openconfig_qos.proto",
        "openconfig_system.proto",
        "yext.proto",
        "ywrapper.proto",
    ],
)

cc_proto_library(
    name = "openconfig_cc_proto",
    deps = [":openconfig_proto"],
)
