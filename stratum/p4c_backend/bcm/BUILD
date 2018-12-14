load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "SANDBLAZE_INTERNAL",
)

package(
    default_copts = [
        "-DDISABLE_GOOGLE_GLOBAL_USING_DECLARATIONS",
    ],
    default_hdrs_check = "warn",
    default_visibility = SANDBLAZE_INTERNAL,
)

cc_library(
    name = "bcm_target_info",
    srcs = [
        "bcm_target_info.cc",
    ],
    hdrs = [
        "bcm_target_info.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//platforms/networking/hercules/p4c_backend/switch:target_info",
        "//platforms/networking/hercules/public/proto:p4_annotation_proto_host",
    ],
)

cc_test(
    name = "bcm_target_info_test",
    srcs = [
        "bcm_target_info_test.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    data = [
        "//platforms/networking/hercules/p4c_backend/switch:testdata/pipeline_opt_block.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":bcm_target_info",
        "//platforms/networking/hercules/public/proto:p4_annotation_proto_host",
        "//testing/base/public:gunit_main_no_google3",
    ],
)

cc_library(
    name = "bcm_tunnel_optimizer",
    srcs = [
        "bcm_tunnel_optimizer.cc",
    ],
    hdrs = [
        "bcm_tunnel_optimizer.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//base",
        "//platforms/networking/hercules/p4c_backend/switch:p4c_switch_utils",
        "//platforms/networking/hercules/p4c_backend/switch:tunnel_optimizer_interface",
        "//platforms/networking/hercules/public/proto:p4_table_defs_proto_host",
        "//protobuf",
    ],
)

cc_test(
    name = "bcm_tunnel_optimizer_test",
    srcs = [
        "bcm_tunnel_optimizer_test.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":bcm_tunnel_optimizer",
        "//platforms/networking/hercules/hal/lib/p4:p4_table_map_proto_host",
        "//platforms/networking/hercules/lib:utils",
        "//platforms/networking/hercules/public/proto:p4_table_defs_proto_host",
        "//testing/base/public:gunit_main_no_google3",
        "//p4lang_p4c:p4c_frontend_midend",
        "//p4lang_p4c/lib:p4c_toolkit",
    ],
)
