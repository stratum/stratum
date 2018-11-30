load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "SANDBLAZE_INTERNAL",
    "sc_cc_lib",
    "HOST_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = SANDBLAZE_INTERNAL,
    # TODO(b/76020938); doesn't work with header modules yet.
    features = ["-use_header_modules"],
)

licenses(["notice"])  # Apache v2

cc_library(
    name = "bcm_sim_test_fixture",
    testonly = 1,
    srcs = ["bcm_sim_test_fixture.cc"],
    hdrs = [
        "bcm_sim_test_fixture.h",
    ],
    data = [
        "//platforms/networking/hercules/hal/bin/bcm/sim:bcm_pcid_sim",
        "//platforms/networking/hercules/hal/config:bcm_sim_config_files",
        "//platforms/networking/hercules/testing/protos:bcm_sim_test_protos",
    ],
    deps = [
        "//base",
        "//devtools/build/runtime:get_runfiles_dir",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_acl_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_chassis_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_l2_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_l3_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_node",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_packetio_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_sdk_sim",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_serdes_db_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_switch",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_table_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_tunnel_manager",
        "//platforms/networking/hercules/hal/lib/common:common_proto",
        "//platforms/networking/hercules/hal/lib/p4:p4_table_mapper",
        "//platforms/networking/hercules/hal/lib/phal:phal_sim",
        "//platforms/networking/hercules/lib:macros",
        "//platforms/networking/hercules/lib:utils",
        "//platforms/networking/hercules/lib/test_utils:matchers",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/synchronization",
        "//sandblaze/p4lang:p4runtime_v1_proto",
    ],
)

sc_cc_lib(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    arches = HOST_ARCHES,
    deps = [
        "//base",
        "//platforms/networking/hercules/glue:init_google",
        "//platforms/networking/hercules/glue:logging",
        "//testing/base/public:gunit_no_google3",
    ],
)

cc_test(
    name = "bcm_sim_test",
    timeout = "long",
    srcs = ["bcm_sim_test.cc"],
    data = [
        "//platforms/networking/hercules/testing/protos:bcm_sim_test_protos",
    ],
    local = 1,
    deps = [
        ":bcm_sim_test_fixture",
        ":test_main",
        "//platforms/networking/hercules/glue:logging",
        "//platforms/networking/hercules/hal/lib/common:writer_interface",
        "//platforms/networking/hercules/lib:macros",
        "//platforms/networking/hercules/lib/test_utils:matchers",
        "//testing/base/public:gunit_no_google3",
        "//absl/strings",
        "//sandblaze/p4lang:p4runtime_v1_proto",
    ],
)
