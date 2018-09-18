load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "SANDBLAZE_INTERNAL",
    "sc_cc_lib",
    "sc_cc_test",
)

package(
    default_hdrs_check = "strict",
    default_visibility = SANDBLAZE_INTERNAL,
)

sc_cc_lib(
    name = "state_machine",
    hdrs = [
        "state_machine.h",
    ],
    deps = [
        "//base",
        "//absl/container:flat_hash_map",
        "//absl/synchronization",
        "//util/task:status",
        "//util/task:statusor",
    ],
)

sc_cc_lib(
    name = "example_state_machine",
    testonly = 1,
    srcs = [
        "example_state_machine.cc",
    ],
    hdrs = [
        "example_state_machine.h",
    ],
    deps = [
        ":state_machine",
        "//testing/base/public:gunit",
        "//absl/container:flat_hash_map",
        "//util/task:status",
    ],
)

sc_cc_test(
    name = "state_machine_test",
    srcs = [
        "state_machine_test.cc",
    ],
    deps = [
        ":example_state_machine",
        ":state_machine",
        "//testing/base/public:gunit",
        "//testing/base/public:gunit_main",
        "//util/task:status",
    ],
)
