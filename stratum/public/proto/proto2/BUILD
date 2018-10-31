# Google3 net/proto2 versions of platforms/networking/hercules/public/proto
# protobufs.

# Including both third-party proto3 libraries & google3 proto libraries
# leads to compile-time name collisions. These build rules generate google3
# proto2 libraries that will compile alongside other google3 proto libraries.
load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

genrule(
    name = "proto2_srcs",
    srcs = ["//stratum/public/proto:proto_srcs"],
    outs = [
        "p4_table_defs.proto",
        "p4_annotation.proto",
    ],
    # Replace:
    #   import "platforms/networking/hercules/.../pkg_proto"
    # with...
    #   import "platforms/networking/hercules/.../proto2/pkg_proto"
    cmd = ("cp $(SRCS) $(@D);" +
           "sed -i 's|\\(import.*platforms/networking/hercules/.*\\)/|\\1/proto2/|' $(OUTS);"),
    visibility = ["//visibility:private"],
)

proto_library(
    name = "p4_table_defs_proto",
    srcs = ["p4_table_defs.proto"],
    #cc_api_version = 2, FIXME(boc) google only
)

proto_library(
    name = "p4_annotation_proto",
    srcs = ["p4_annotation.proto"],
    #cc_api_version = 2, FIXME(boc) google only
    deps = [":p4_table_defs_proto"],
)
