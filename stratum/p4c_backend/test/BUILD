# This BUILD file provides common test support for the Hercules p4c backend.
# It also creates a simple p4c test binary with limited backend
# functionality, then uses it to test p4c with the comprehensive set of
# sample files in //p4lang_p4c.

load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "SANDBLAZE_INTERNAL",
)
load(
    "//platforms/networking/hercules/p4c_backend/test:build_defs.bzl",
    "p4c_save_ir",
)

package(
    default_copts = [
        "-DDISABLE_GOOGLE_GLOBAL_USING_DECLARATIONS",
    ],
    default_hdrs_check = "warn",
    default_visibility = SANDBLAZE_INTERNAL,
)

cc_library(
    name = "ir_test_helpers",
    srcs = [
        "ir_test_helpers.cc",
    ],
    hdrs = [
        "ir_test_helpers.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//base",
        "//platforms/networking/hercules/p4c_backend/common:backend_extension_interface",
        "//platforms/networking/hercules/p4c_backend/common:backend_pass_manager",
        "//platforms/networking/hercules/p4c_backend/common:p4c_front_mid_real",
        "//platforms/networking/hercules/p4c_backend/common:program_inspector",
        "//platforms/networking/hercules/p4c_backend/switch:hit_assign_mapper",
        "//platforms/networking/hercules/p4c_backend/switch:meter_color_mapper",
        "//absl/memory",
        "//p4lang_p4c:control_plane_h",
        "//p4lang_p4c:p4c_frontend_midend",
        "//p4lang_p4c:p4c_ir",
        "//p4lang_p4c/backends/p4test:p4c_common_midend_pass",
        "//p4lang_p4c/lib:p4c_toolkit",
        "//sandblaze/p4lang:p4info_v1_proto_host",
    ],
)

cc_test(
    name = "ir_test_helpers_test",
    srcs = [
        "ir_test_helpers_test.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    data = [
        "testdata/simple_vlan_stack_16.ir.json",
        "//platforms/networking/hercules/p4c_backend/switch:testdata/hit_assign.ir.json",
        "//platforms/networking/hercules/p4c_backend/switch:testdata/meter_color_errors1.ir.json",
        "//platforms/networking/hercules/p4c_backend/switch:testdata/meter_colors.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":ir_test_helpers",
        "//platforms/networking/hercules/p4c_backend/switch:p4c_switch_utils",
        "//testing/base/public:gunit_main_no_google3",
        "//p4lang_p4c:p4c_testdata_files",
    ],
)

# Inspects IR nodes to collect data for unit tests.
cc_library(
    name = "test_inspectors",
    hdrs = [
        "test_inspectors.h",
    ],
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
    name = "test_inspectors_test",
    srcs = [
        "test_inspectors_test.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    data = [
        "//platforms/networking/hercules/p4c_backend/switch:testdata/pipeline_opt_block.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":ir_test_helpers",
        ":test_inspectors",
        "//platforms/networking/hercules/p4c_backend/switch:p4c_switch_utils",
        "//platforms/networking/hercules/public/proto:p4_annotation_proto_host",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//p4lang_p4c:p4c_ir",
    ],
)

cc_library(
    name = "test_target_info",
    srcs = [
        "test_target_info.cc",
    ],
    hdrs = [
        "test_target_info.h",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//platforms/networking/hercules/p4c_backend/switch:target_info",
    ],
)

cc_test(
    name = "test_target_info_test",
    srcs = [
        "test_target_info_test.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    data = [
        "//platforms/networking/hercules/p4c_backend/switch:testdata/pipeline_opt_block.ir.json",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        ":test_target_info",
        "//platforms/networking/hercules/public/proto:p4_annotation_proto_host",
        "//testing/base/public:gunit_main_no_google3",
    ],
)

# This p4c binary generates IR data in JSON format for unit tests.
cc_binary(
    name = "p4c_ir_json_saver",
    srcs = [
        "p4c_ir_json_saver.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//base",
        "//platforms/networking/hercules/p4c_backend/common:backend_extension_interface",
        "//platforms/networking/hercules/p4c_backend/common:backend_pass_manager",
        "//platforms/networking/hercules/p4c_backend/common:p4c_front_mid_real",
        "//platforms/networking/hercules/p4c_backend/switch:midend",
    ],
)

# This p4c binary runs from the p4c_sample_files_test.
cc_binary(
    name = "test_p4c",
    srcs = [
        "test_p4c_main.cc",
    ],
    copts = [
        "-fexceptions",
    ],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    deps = [
        "//base",
        "//platforms/networking/hercules/p4c_backend/common:backend_extension_interface",
        "//platforms/networking/hercules/p4c_backend/common:backend_pass_manager",
        "//platforms/networking/hercules/p4c_backend/common:p4c_front_mid_real",
    ],
)

# This test must execute with gunit_main in order to access the test file
# lists through FLAGS_test_srcdir.  For this to work, it needs to avoid
# direct or indirect sandblaze dependencies.  The local flag is set because
# p4c forks the C preprocessor, which isn't available in the blaze test
# environment.
cc_test(
    name = "p4c_sample_files_test",
    size = "medium",
    timeout = "moderate",
    srcs = ["p4c_sample_files_test.cc"],
    copts = [
        "-fexceptions",
    ],
    data = [":test_p4c"],
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    local = 1,
    deps = [
        "//base",
        "//testing/base/public:gunit_main",
        "//p4lang_p4c:p4_16_test_file_list",
        "//p4lang_p4c:p4c_testdata_files",
    ],
)

p4c_save_ir(
    name = "simple_vlan_stack",
    src = "testdata/simple_vlan_stack_16.p4",
    out_ir = "testdata/simple_vlan_stack_16.ir.json",
)
