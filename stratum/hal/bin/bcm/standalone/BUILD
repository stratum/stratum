load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "SANDBLAZE_INTERNAL",
    "sc_cc_bin",
    "sc_package",
    "EMBEDDED_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = SANDBLAZE_INTERNAL,
)

licenses(["notice"])  # Apache v2

sc_cc_bin(
    name = "hercules",
    srcs = [
        "main.cc",
    ],
    arches = EMBEDDED_ARCHES,
    copts = [
        "-lpthread",
        "-ldl",
        "-lrt",
        "-lutil",
    ],
    deps = [
        "//base",
        "//platforms/networking/hercules/glue:init_google",
        "//platforms/networking/hercules/glue:logging",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_acl_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_chassis_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_diag_shell",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_l2_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_l3_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_node",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_packetio_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_sdk_wrapper",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_serdes_db_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_switch",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_table_manager",
        "//platforms/networking/hercules/hal/lib/bcm:bcm_tunnel_manager",
        "//platforms/networking/hercules/hal/lib/common:hal",
        "//platforms/networking/hercules/hal/lib/p4:p4_table_mapper",
        "//platforms/networking/hercules/hal/lib/phal:legacy_phal",
        "//platforms/networking/hercules/hal/lib/phal:udev",
        "//platforms/networking/hercules/lib/security:auth_policy_checker",
        "//platforms/networking/hercules/lib/security:credentials_manager",
        "//absl/memory",
        "//absl/synchronization",
    ],
)

sc_package(
    name = "hercules_pkg",
    bins = [
        ":hercules",
    ],
)
