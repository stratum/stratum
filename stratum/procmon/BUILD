load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "SANDBLAZE_INTERNAL",
    "sc_cc_bin",
    "sc_cc_lib",
    "sc_cc_test",
    "sc_package",
    "sc_proto_lib",
)

package(
    default_hdrs_check = "strict",
    default_visibility = SANDBLAZE_INTERNAL,
)

sc_proto_lib(
    name = "procmon_proto",
    srcs = ["procmon.proto"],
    services = ["grpc"],
)

sc_cc_lib(
    name = "procmon",
    srcs = ["procmon.cc"],
    hdrs = ["procmon.h"],
    deps = [
        ":procmon_proto",
        "//base",
        "//platforms/networking/hercules/lib:macros",
        "//platforms/networking/hercules/lib:utils",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//util/task:status",
    ],
)

sc_cc_lib(
    name = "procmon_service_impl",
    srcs = ["procmon_service_impl.cc"],
    hdrs = ["procmon_service_impl.h"],
    deps = [
        ":procmon_grpc_proto",
        "//sandblaze/prebuilt/grpc",
    ],
)

sc_cc_test(
    name = "procmon_test",
    srcs = ["procmon_test.cc"],
    deps = [
        ":procmon",
        ":procmon_proto",
        "//base",
        "//platforms/networking/hercules/lib:utils",
        "//platforms/networking/hercules/lib/test_utils:matchers",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//absl/synchronization",
    ],
)

sc_cc_bin(
    name = "procmon_main",
    srcs = ["procmon_main.cc"],
    copts = [
        "-lpthread",
        "-ldl",
        "-lrt",
    ],
    deps = [
        ":procmon",
        ":procmon_proto",
        ":procmon_service_impl",
        "//base",
        "//platforms/networking/hercules/glue:init_google",
        "//platforms/networking/hercules/glue:logging",
        "//platforms/networking/hercules/lib:constants",
        "//platforms/networking/hercules/lib:macros",
        "//platforms/networking/hercules/lib:utils",
        "//sandblaze/prebuilt/grpc",
        "//sandblaze/prebuilt/openssl",
        "//util/task:status",
    ],
)

sc_package(
    name = "procmon_main_pkg",
    bins = [
        ":procmon_main",
    ],
)
