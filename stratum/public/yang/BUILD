'''FIXME(boc) GOOGLE ONLY
# Collections of YANG models to use as dependencies
load("//ops/netops/openconfig/build:yang.bzl", "yang_library")
load("//ops/netops/openconfig/build:yang.bzl", "yang_lint_test")

licenses(["notice"])  # Apache v2

yang_library(
    name = "hercules_interfaces",
    srcs = [
        "openconfig-hercules-interfaces.yang",
    ],
)

yang_library(
    name = "hercules_platform",
    srcs = [
        "openconfig-hercules-platform.yang",
        "openconfig-hercules-platform-asic-bcm.yang",
        "openconfig-hercules-platform-chassis.yang",
        "openconfig-hercules-platform-google.yang",
        "openconfig-hercules-platform-linecard.yang",
        "openconfig-hercules-platform-node.yang",
        "openconfig-hercules-platform-port.yang",
    ],
)

yang_library(
    name = "hercules_extensions",
    srcs = [
        "openconfig-hercules.yang",
    ],
)

yang_library(
    name = "hercules_qos",
    srcs = [
        "openconfig-hercules-qos.yang",
    ],
)

# YANG models of objects that are exposed by a Vendor Agnostic Stack
yang_lint_test(
    name = "hercules",
    src = "openconfig-hercules.yang",
    warnings_are_errors = False,
    deps = [
        "//openconfig/public/release/models:openconfig_modules",
    ],
)

yang_lint_test(
    name = "interfaces",
    src = "openconfig-hercules-interfaces.yang",
    warnings_are_errors = False,
    deps = [
        "//stratum/public/yang:hercules_extensions",
        "//stratum/public/yang:hercules_interfaces",
        "//openconfig/ietf:ietf_yang_modules",
        "//openconfig/public/release/models:openconfig_modules",
    ],
)

yang_lint_test(
    name = "platform",
    src = "openconfig-hercules-platform.yang",
    warnings_are_errors = False,
    deps = [
        "//stratum/public/yang:hercules_extensions",
        "//stratum/public/yang:hercules_platform",
        "//openconfig/ietf:ietf_yang_modules",
        "//openconfig/public/release/models:openconfig_modules",
    ],
)

yang_lint_test(
    name = "qos",
    src = "openconfig-hercules-qos.yang",
    warnings_are_errors = False,
    deps = [
        "//stratum/public/yang:hercules_qos",
        "//openconfig/ietf:ietf_yang_modules",
        "//openconfig/public/release/models:openconfig_modules",
    ],
)
  end google only'''