# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
)
load(":platform_config_test.bzl", "platform_config_test")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(default_visibility = STRATUM_INTERNAL)

licenses(["notice"])  # Apache v2

platform_config_test(
    "x86-64-accton-as7712-32x-r0",
    bcm_target = True,
)

platform_config_test(
    "x86-64-accton-as7716-24sc-r0",
    bcm_target = True,
)

platform_config_test(
    "x86-64-accton-as7716-24xc-r0",
    bcm_target = True,
)

platform_config_test(
    "x86-64-accton-as6712-32x-r0",
    bcm_target = True,
)

platform_config_test(
    "x86-64-dell-z9100-c2538-r0",
    bcm_target = True,
)

platform_config_test(
    "x86-64-inventec-d7032q28b-r0",
    bcm_target = True,
)

platform_config_test(
    "x86-64-inventec-d7054q28b-r0",
    bcm_target = True,
)

platform_config_test(
    "x86-64-inventec-d6254qs-r0",
    bcm_target = True,
)

platform_config_test(
    "x86-64-quanta-ix1-rangeley-r0",
    bcm_target = True,
)

platform_config_test(
    "barefoot-tofino-model",
    sim_target = True,
)

platform_config_test(
    "p4-dpdk-software-switch",
    sim_target = True,
)

platform_config_test(
    "x86-64-accton-wedge100bf-32x-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-accton-wedge100bf-32qs-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-accton-wedge100bf-65x-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-delta-ag9064v1-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-inventec-d5254-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-inventec-d5264q28b-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-netberg-aurora-610-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-netberg-aurora-710-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-netberg-aurora-750-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-stordis-bf2556x-1t-r0",
    tofino_target = True,
)

platform_config_test(
    "x86-64-stordis-bf6064x-t-r0",
    tofino_target = True,
)

filegroup(
    name = "platform_configs",
    srcs = glob([
        "*/base_bcm_chassis_map.pb.txt",
        "*/chassis_config.pb.txt",
        "*/phal_config.pb.txt",
        "*/SDKLT.yml",
        "*/config.bcm",
        "*/port_map.json",
    ]) + [
        "bcm_hardware_specs.pb.txt",
        "dummy_serdes_db.pb.txt",
        "pipeline_cfg.pb.txt",
    ],
)

pkg_tar(
    name = "platform_configs_tar",
    srcs = [":platform_configs"],
    extension = "tar.xz",
    mode = "0644",
    strip_prefix = ".",  # needed to preserve sub-directory structure: https://github.com/bazelbuild/rules_pkg/issues/82
)

stratum_cc_binary(
    name = "chassis_config_migrator",
    srcs = ["chassis_config_migrator.cc"],
    data = [":platform_configs"],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib:utils",
        "@com_google_absl//absl/strings",
    ],
)
