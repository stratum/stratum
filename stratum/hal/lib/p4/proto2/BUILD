# Google3 net/proto2 versions of platforms/networking/hercules/hal/lib/p4
# protobufs.

# Including both third-party proto3 libraries & google3 proto libraries
# leads to compile-time name collisions. These build rules generate google3
# proto2 libraries that will compile alongside other google3 proto libraries.

load(
    "//bazel:rules.bzl",
    "SANDBLAZE_INTERNAL",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = SANDBLAZE_INTERNAL,
)

genrule(
    name = "proto2_srcs",
    srcs = ["//stratum/hal/lib/p4:proto_srcs"],
    outs = [
        "common_flow_entry.proto",
        "p4_control.proto",
        "p4_pipeline_config.proto",
        "p4_table_map.proto",
    ],
    # Replace:
    #   import "platforms/networking/hercules/.../pkg_proto"
    #   import "p4/pkg_proto"
    # with...
    #   import "platforms/networking/hercules/.../proto2/pkg_proto"
    #   import "p4lang_pi/proto/p4/pkg_proto"
    cmd = ("cp $(SRCS) $(@D);" +
           "sed -i 's|\\(import.*platforms/networking/hercules/.*\\)/|\\1/proto2/|' $(OUTS);" +
           "sed -i 's|\\(import *\"\\)p4/|\\1p4lang_PI/proto/p4/|' $(OUTS);"),
    visibility = ["//visibility:private"],
)

proto_library(
    name = "p4_control_proto",
    srcs = ["p4_control.proto"],
    cc_api_version = 2,
    deps = [
        "//stratum/public/proto/proto2:p4_annotation_proto",
        "//stratum/public/proto/proto2:p4_table_defs_proto",
    ],
)

proto_library(
    name = "common_flow_entry_proto",
    srcs = ["common_flow_entry.proto"],
    cc_api_version = 2,
    deps = [
        "//stratum/public/proto/proto2:p4_annotation_proto",
        "//stratum/public/proto/proto2:p4_table_defs_proto",
        "//p4lang_PI/proto/p4/v1:p4runtime_proto",
    ],
)

proto_library(
    name = "p4_table_map_proto",
    srcs = ["p4_table_map.proto"],
    cc_api_version = 2,
    deps = [
        ":common_flow_entry_proto",
        "//stratum/public/proto/proto2:p4_annotation_proto",
        "//stratum/public/proto/proto2:p4_table_defs_proto",
        "//p4lang_PI/proto/p4/config/v1:p4info_proto",
    ],
)

proto_library(
    name = "p4_pipeline_config_proto",
    srcs = ["p4_pipeline_config.proto"],
    cc_api_version = 2,
    deps = [
        ":p4_control_proto",
        ":p4_table_map_proto",
        "//stratum/public/proto/proto2:p4_annotation_proto",
        "//p4lang_PI/proto/p4/v1:p4runtime_proto",
    ],
)
