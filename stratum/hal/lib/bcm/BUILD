load(
    "//stratum/portage:build_defs.bzl",
    "STRATUM_INTERNAL",
    "sc_cc_lib",
    "sc_cc_test",
    "sc_proto_lib",
    "HOST_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

licenses(["notice"])  # Apache v2

sc_proto_lib(
    name = "bcm_proto",
    hdrs = ["bcm.proto"],
    deps = [
        "//stratum/hal/lib/common:common_proto",
    ],
)

sc_cc_lib(
    name = "bcm_global_vars",
    srcs = ["bcm_global_vars.cc"],
    hdrs = ["bcm_global_vars.h"],
    deps = [
        "//absl/synchronization",
    ],
)

sc_cc_lib(
    name = "bcm_acl_manager",
    srcs = ["bcm_acl_manager.cc"],
    hdrs = ["bcm_acl_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_chassis_ro_interface",
        ":bcm_proto",
        ":bcm_sdk_interface",
        ":bcm_table_manager",
        ":pipeline_processor",
        "//base",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:p4_control_proto",
        "//stratum/hal/lib/p4:p4_pipeline_config_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_annotation_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "bcm_acl_manager_test",
    srcs = ["bcm_acl_manager_test.cc"],
    deps = [
        ":bcm_acl_manager",
        ":bcm_chassis_manager_mock",
        ":bcm_sdk_mock",
        ":bcm_table_manager_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/strings",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/proto:p4_annotation_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:flat_map",
        "//util/gtl:flat_set",
        "//util/gtl:map_util",
    ],
)

cc_library(
    name = "bcm_acl_manager_mock",
    testonly = 1,
    hdrs = ["bcm_acl_manager_mock.h"],
    deps = [
        ":bcm_acl_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_lib(
    name = "bcm_chassis_ro_interface",
    hdrs = ["bcm_chassis_ro_interface.h"],
    deps = [
        ":bcm_proto",
        ":utils",
        "//absl/base:core_headers",
        "//absl/strings",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:utils",
    ],
)

sc_cc_lib(
    name = "bcm_chassis_ro_mock",
    testonly = 1,
    hdrs = ["bcm_chassis_ro_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_chassis_ro_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_lib(
    name = "bcm_chassis_manager",
    srcs = ["bcm_chassis_manager.cc"],
    hdrs = ["bcm_chassis_manager.h"],
    deps = [
        ":bcm_chassis_ro_interface",
        ":bcm_global_vars",
        ":bcm_node",
        ":bcm_proto",
        ":bcm_sdk_interface",
        ":bcm_serdes_db_manager",
        ":utils",
        "//base",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/memory",
        "//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:utils",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/public/lib:error",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
    name = "bcm_chassis_manager_mock",
    testonly = 1,
    hdrs = ["bcm_chassis_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_chassis_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_chassis_manager_test",
    srcs = ["bcm_chassis_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_node_mock",
        ":bcm_sdk_mock",
        ":bcm_serdes_db_manager_mock",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "bcm_diag_shell",
    srcs = ["bcm_diag_shell.cc"],
    hdrs = ["bcm_diag_shell.h"],
    defines = ["NDEBUG"],
    deps = [
        ":sdk_build_undef",
        "//base",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//sandblaze/prebuilt/broadcom:bcm-sdk",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "bcm_flow_table",
    hdrs = ["bcm_flow_table.h"],
    deps = [
        "//absl/base:core_headers",
        "//absl/container:node_hash_set",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "//util/gtl:node_hash_set",
    ],
)

sc_cc_test(
    name = "bcm_flow_table_test",
    srcs = ["bcm_flow_table_test.cc"],
    deps = [
        ":bcm_flow_table",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "bcm_l2_manager",
    srcs = ["bcm_l2_manager.cc"],
    hdrs = ["bcm_l2_manager.h"],
    deps = [
        ":bcm_chassis_ro_interface",
        ":bcm_proto",
        ":bcm_sdk_interface",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:macros",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

sc_cc_lib(
    name = "bcm_l2_manager_mock",
    testonly = 1,
    hdrs = ["bcm_l2_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_l2_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_l2_manager_test",
    srcs = ["bcm_l2_manager_test.cc"],
    deps = [
        ":bcm_chassis_ro_mock",
        ":bcm_l2_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "bcm_l3_manager",
    srcs = ["bcm_l3_manager.cc"],
    hdrs = ["bcm_l3_manager.h"],
    deps = [
        ":bcm_proto",
        ":bcm_sdk_interface",
        ":bcm_table_manager",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/memory",
        "//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "bcm_l3_manager_mock",
    testonly = 1,
    hdrs = ["bcm_l3_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_l3_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_l3_manager_test",
    srcs = ["bcm_l3_manager_test.cc"],
    deps = [
        ":bcm_l3_manager",
        ":bcm_sdk_mock",
        ":bcm_table_manager_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "//util/gtl:source_location",
    ],
)

sc_cc_lib(
    name = "bcm_packetio_manager",
    srcs = ["bcm_packetio_manager.cc"],
    hdrs = ["bcm_packetio_manager.h"],
    deps = [
        ":bcm_chassis_ro_interface",
        ":bcm_global_vars",
        ":bcm_proto",
        ":bcm_sdk_interface",
        ":constants",
        "//base",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
    name = "bcm_packetio_manager_mock",
    testonly = 1,
    hdrs = ["bcm_packetio_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_packetio_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_packetio_manager_test",
    srcs = ["bcm_packetio_manager_test.cc"],
    deps = [
        ":bcm_chassis_ro_mock",
        ":bcm_packetio_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "//util/libcproxy:libcwrapper",
        "//util/libcproxy:passthrough_proxy",
    ],
)

sc_cc_lib(
    name = "bcm_sdk_proxy",
    srcs = ["bcm_sdk_proxy.cc"],
    hdrs = ["bcm_sdk_proxy.h"],
    deps = [
        ":bcm_sdk_interface",
        ":constants",
        "//base",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:client_sync_reader_writer",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/sandcastle:bcm_sdk_service_grpc_proto",
        "//stratum/lib/sandcastle:hardware_status_service_grpc_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_test(
    name = "bcm_sdk_proxy_test",
    srcs = ["bcm_sdk_proxy_test.cc"],
    data = ["//tools/test:requires_ipv4_loopback"],
    deps = [
        ":bcm_sdk_proxy",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/synchronization",
        "//absl/time",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:hardware_status_service_test_helper",
        "//stratum/lib:utils",
        "//stratum/lib/sandcastle:hardware_status_service_grpc_proto",
        "//stratum/public/proto:error_proto",
    ],
)

sc_cc_lib(
    name = "bcm_sdk_interface",
    hdrs = ["bcm_sdk_interface.h"],
    deps = [
        ":bcm_proto",
        "//absl/base:core_headers",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/lib/channel",
    ],
)

sc_cc_lib(
    name = "bcm_sdk_mock",
    testonly = 1,
    hdrs = ["bcm_sdk_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_sdk_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_lib(
    name = "bcm_sdk_sim",
    srcs = ["bcm_sdk_sim.cc"],
    hdrs = ["bcm_sdk_sim.h"],
    deps = [
        ":bcm_sdk_wrapper",
        ":macros",
        ":sdk_build_undef",
        "//base",
        "//absl/base:core_headers",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/broadcom:bcm-sdk",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
    name = "bcm_sdk_wrapper",
    srcs = ["bcm_sdk_wrapper.cc"],
    hdrs = ["bcm_sdk_wrapper.h"],
    defines = ["NDEBUG"],
    deps = [
        ":bcm_diag_shell",
        ":bcm_sdk_interface",
        ":constants",
        ":macros",
        ":sdk_build_undef",
        "//base",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/strings",
        "//absl/synchronization",
        "//protobuf",
        "//sandblaze/prebuilt/broadcom:bcm-sdk",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//util/endian",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
    name = "bcm_serdes_db_manager",
    srcs = ["bcm_serdes_db_manager.cc"],
    hdrs = ["bcm_serdes_db_manager.h"],
    deps = [
        ":bcm_proto",
        ":utils",
        "//base",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
    ],
)

sc_cc_lib(
    name = "bcm_serdes_db_manager_mock",
    testonly = 1,
    hdrs = ["bcm_serdes_db_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_serdes_db_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_serdes_db_manager_test",
    srcs = ["bcm_serdes_db_manager_test.cc"],
    deps = [
        ":bcm_serdes_db_manager",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "bcm_node",
    srcs = ["bcm_node.cc"],
    hdrs = ["bcm_node.h"],
    deps = [
        ":bcm_acl_manager",
        ":bcm_global_vars",
        ":bcm_l2_manager",
        ":bcm_l3_manager",
        ":bcm_packetio_manager",
        ":bcm_table_manager",
        ":bcm_tunnel_manager",
        ":constants",
        "//base",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/synchronization",
        "//protobuf",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "bcm_node_mock",
    testonly = 1,
    hdrs = ["bcm_node_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_node",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_node_test",
    srcs = ["bcm_node_test.cc"],
    deps = [
        ":bcm_acl_manager_mock",
        ":bcm_l2_manager_mock",
        ":bcm_l3_manager_mock",
        ":bcm_node",
        ":bcm_packetio_manager_mock",
        ":bcm_table_manager_mock",
        ":bcm_tunnel_manager_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
    ],
)

sc_cc_lib(
    name = "bcm_switch",
    srcs = ["bcm_switch.cc"],
    hdrs = ["bcm_switch.h"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_global_vars",
        ":bcm_node",
        ":constants",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/synchronization",
        "//absl/time",
        "//protobuf",
        "//stratum:status_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "bcm_switch_test",
    srcs = ["bcm_switch_test.cc"],
    deps = [
        ":bcm_chassis_manager_mock",
        ":bcm_node_mock",
        ":bcm_packetio_manager_mock",
        ":bcm_switch",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "bcm_table_manager",
    srcs = ["bcm_table_manager.cc"],
    hdrs = ["bcm_table_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_chassis_ro_interface",
        ":bcm_flow_table",
        ":bcm_proto",
        ":constants",
        ":utils",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//protobuf",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:common_flow_entry_proto",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
    name = "bcm_table_manager_mock",
    testonly = 1,
    hdrs = ["bcm_table_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_table_manager",
        "//testing/base/public:gunit_no_google3",
        "//absl/container:flat_hash_map",
    ],
)

sc_cc_test(
    name = "bcm_table_manager_test",
    srcs = ["bcm_table_manager_test.cc"],
    deps = [
        ":bcm_chassis_ro_mock",
        ":bcm_table_manager",
        ":constants",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/memory",
        "//absl/strings",
        "//protobuf",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "constants",
    hdrs = ["constants.h"],
    deps = [
        "//absl/base:core_headers",
    ],
)

sc_cc_lib(
    name = "macros",
    hdrs = ["macros.h"],
    defines = ["NDEBUG"],
    deps = [
        "//sandblaze/prebuilt/broadcom:bcm-sdk",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "macros_test",
    srcs = ["macros_test.cc"],
    deps = [
        ":macros",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "sdk_build_undef",
    hdrs = ["sdk_build_undef.h"],
)

sc_cc_lib(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    arches = HOST_ARCHES,
    deps = [
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)

sc_cc_lib(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":bcm_proto",
        "//absl/base:core_headers",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:constants",
    ],
)

sc_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "//testing/base/public:gunit_no_google3",
        "//stratum/lib:constants",
    ],
)

sc_cc_lib(
    name = "acl_table",
    srcs = ["acl_table.cc"],
    hdrs = ["acl_table.h"],
    deps = [
        ":bcm_flow_table",
        ":bcm_proto",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//stratum/hal/lib/p4:common_flow_entry_proto",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "acl_table_test",
    srcs = ["acl_table_test.cc"],
    deps = [
        ":acl_table",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/container:flat_hash_map",
        "//sandblaze/p4lang:p4runtime_v1_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "pipeline_processor",
    srcs = ["pipeline_processor.cc"],
    hdrs = ["pipeline_processor.h"],
    deps = [
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/container:node_hash_map",
        "//absl/memory",
        "//absl/strings",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//stratum/hal/lib/p4:common_flow_entry_proto",
        "//stratum/hal/lib/p4:p4_control_proto",
        "//stratum/hal/lib/p4:p4_table_map_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
        "//util/gtl:node_hash_map",
        "//util/task:status",
        "//util/task:statusor",
    ],
)

sc_cc_test(
    name = "pipeline_processor_test",
    srcs = ["pipeline_processor_test.cc"],
    deps = [
        ":pipeline_processor",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/container:flat_hash_map",
        "//absl/strings",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/lib/test_utils:p4_proto_builders",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_annotation_proto",
    ],
)

sc_cc_lib(
    name = "bcm_udf_manager",
    srcs = ["bcm_udf_manager.cc"],
    hdrs = ["bcm_udf_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_proto",
        ":bcm_sdk_interface",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "bcm_udf_manager_test",
    srcs = ["bcm_udf_manager_test.cc"],
    deps = [
        ":bcm_sdk_mock",
        ":bcm_udf_manager",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/strings",
        "//absl/strings:str_format",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//util/endian",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:flat_map",
        "//util/gtl:flat_set",
    ],
)
