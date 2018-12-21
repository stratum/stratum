load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "SANDBLAZE_INTERNAL",
)

package(
    default_copts = [
        "-DDISABLE_GOOGLE_GLOBAL_USING_DECLARATIONS",
    ],
    default_hdrs_check = "warn",
    default_visibility = SANDBLAZE_INTERNAL + [
        "//platforms/networking/orion/p4:__subpackages__",
    ],
)

cc_library(
    name = "p4c_front_mid_interface",
    textual_hdrs = [
        "p4c_front_mid_interface.h",
    ],
)

cc_library(
    name = "backend_extension_interface",
    textual_hdrs = [
        "backend_extension_interface.h",
    ],
    deps = [
        "//sandblaze/p4lang:p4info_v1_proto_host",
        "//sandblaze/p4lang:p4runtime_v1_proto_host",
    ],
)

cc_library(
    name = "midend_interface",
    textual_hdrs = [
        "midend_interface.h",
    ],
)

cc_library(
    name = "p4c_common_mocks",
    textual_hdrs = [
        "backend_extension_mock.h",
        "p4c_front_mid_mock.h",
    ],
)

cc_library(
    name = "p4c_front_mid_real",
    srcs = [
        "p4c_front_mid_real.cc",
    ],
    hdrs = [
        "p4c_front_mid_real.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":midend_interface",
        ":midend_p4c_open",
        ":p4c_front_mid_interface",
        "//base",
        "//absl/debugging:leak_check",
        "//absl/memory",
        "//p4lang_p4c:control_plane",
        "//p4lang_p4c:control_plane_h",
        "//p4lang_p4c:p4c_frontend_midend",
        "//p4lang_p4c/lib:p4c_toolkit",
    ],
)

cc_library(
    name = "backend_pass_manager",
    srcs = [
        "backend_pass_manager.cc",
    ],
    hdrs = [
        "backend_pass_manager.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":backend_extension_interface",
        ":p4c_front_mid_interface",
        "//base",
        "//platforms/networking/hercules/lib:utils",
        "//p4lang_p4c:p4c_frontend_midend",
        "//p4lang_p4c:p4c_ir",
        "//p4lang_p4c/backends/p4test:p4c_common_midend_pass",
        "//p4lang_p4c/lib:p4c_toolkit",
        "//protobuf",
        "//sandblaze/p4lang:p4info_v1_proto_host",
        "//sandblaze/p4lang:p4runtime_v1_proto_host",
    ],
)

cc_test(
    name = "backend_pass_manager_test",
    srcs = ["backend_pass_manager_test.cc"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":backend_pass_manager",
        ":p4c_common_mocks",
        "//base",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//p4lang_p4c:p4c_frontend_midend",
        "//p4lang_p4c:p4c_ir",
    ],
)

cc_library(
    name = "midend_p4c_open",
    srcs = ["midend_p4c_open.cc"],
    hdrs = ["midend_p4c_open.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":midend_interface",
        "//base",
        "//p4lang_p4c:p4c_frontend_midend",
        "//p4lang_p4c:p4c_ir",
        "//p4lang_p4c/backends/p4test:p4c_common_midend_pass",
    ],
)

cc_test(
    name = "midend_p4c_open_test",
    srcs = ["midend_p4c_open_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "//platforms/networking/hercules/p4c_backend/test:testdata/simple_vlan_stack_16.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":midend_p4c_open",
        "//base",
        "//platforms/networking/hercules/p4c_backend/test:ir_test_helpers",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//p4lang_p4c:p4c_frontend_midend",
        "//p4lang_p4c:p4c_ir",
    ],
)

cc_library(
    name = "program_inspector",
    srcs = ["program_inspector.cc"],
    hdrs = ["program_inspector.h"],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//base",
        "//p4lang_p4c:p4c_frontend_midend",
        "//p4lang_p4c:p4c_ir",
    ],
)

cc_test(
    name = "program_inspector_test",
    srcs = ["program_inspector_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [
        "//platforms/networking/hercules/p4c_backend/switch:testdata/tor_p4.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":program_inspector",
        "//base",
        "//platforms/networking/hercules/p4c_backend/test:ir_test_helpers",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//p4lang_p4c:p4c_frontend_midend",
        "//p4lang_p4c:p4c_ir",
    ],
)
