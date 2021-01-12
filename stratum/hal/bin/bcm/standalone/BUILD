# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# Copyright 2019 Broadcom. All rights reserved. The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_deb", "pkg_tar")
load(
    "//bazel:rules.bzl",
    "EMBEDDED_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_package",
)
load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_bundle",
    "container_image",
    "container_push",
)
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit")

licenses(["notice"])  # Apache v2

exports_files(["LICENSE"])

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_bcm_common_deps = [
    "@com_github_google_glog//:glog",
    "@com_google_absl//absl/memory",
    "@com_google_absl//absl/synchronization",
    "//stratum/glue:init_google",
    "//stratum/glue:logging",
    "//stratum/hal/lib/bcm:bcm_acl_manager",
    "//stratum/hal/lib/bcm:bcm_chassis_manager",
    "//stratum/hal/lib/bcm:bcm_l2_manager",
    "//stratum/hal/lib/bcm:bcm_l3_manager",
    "//stratum/hal/lib/bcm:bcm_node",
    "//stratum/hal/lib/bcm:bcm_packetio_manager",
    "//stratum/hal/lib/bcm:bcm_serdes_db_manager",
    "//stratum/hal/lib/bcm:bcm_switch",
    "//stratum/hal/lib/bcm:bcm_table_manager",
    "//stratum/hal/lib/bcm:bcm_tunnel_manager",
    "//stratum/hal/lib/common:hal",
    "//stratum/hal/lib/p4:p4_table_mapper",
    "//stratum/hal/lib/phal:phal",
    "//stratum/lib/security:auth_policy_checker",
    "//stratum/lib/security:credentials_manager",
]

stratum_cc_binary(
    name = "stratum_bcm_sdklt",
    srcs = [
        "main.cc",
    ],
    arches = EMBEDDED_ARCHES,
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lutil",
        "-lrt",
        "-lyaml",
    ],
    deps = stratum_bcm_common_deps + [
        "//stratum/hal/lib/bcm/sdklt:bcm_sdk_wrapper",
        "//stratum/hal/lib/bcm/sdklt:bcm_diag_shell",
    ],
)

stratum_cc_binary(
    name = "stratum_bcm_opennsa",
    srcs = [
        "main.cc",
    ],
    arches = EMBEDDED_ARCHES,
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lutil",
    ],
    deps = stratum_bcm_common_deps + [
        "//stratum/hal/lib/bcm/sdk:bcm_sdk_wrapper",
        "//stratum/hal/lib/bcm/sdk:bcm_diag_shell",
    ],
)

stratum_package(
    name = "stratum_pkg",
    bins = [
        ":stratum_hal",
    ],
)

pkg_tar(
    name = "stratum_bcm_sdklt_bin",
    srcs = [
        "sdklt/start-stratum.sh",
        ":stratum_bcm_sdklt",
    ],
    mode = "0755",
    package_dir = "/usr/bin",
)

pkg_tar(
    name = "stratum_bcm_opennsa_bin",
    srcs = [
        "sdk/start-stratum.sh",
        ":stratum_bcm_opennsa",
    ],
    mode = "0755",
    package_dir = "/usr/bin",
)

pkg_tar(
    name = "stratum_bcm_sdklt_lib",
    srcs = [
        "@com_github_opennetworkinglab_sdklt//:kernel_modules",
    ],
    package_dir = "/usr/lib/stratum",
)

pkg_tar(
    name = "stratum_bcm_opennsa_lib",
    srcs = [
        "@com_github_broadcom_opennsa//:kernel_modules",
    ],
    package_dir = "/usr/lib/stratum",
)

pkg_tar(
    name = "stratum_bcm_etc",
    srcs = [
        "//stratum/hal/lib/common:gnmi_caps.pb.txt",
    ],
    package_dir = "/etc/stratum",
    deps = [
        "//stratum/hal/config:platform_configs_tar",
    ],
)

pkg_tar(
    name = "statum_bcm_sdklt_all",
    deps = [
        ":stratum_bcm_etc",
        ":stratum_bcm_sdklt_bin",
        ":stratum_bcm_sdklt_lib",
    ],
)

pkg_tar(
    name = "statum_bcm_opennsa_all",
    deps = [
        ":stratum_bcm_etc",
        ":stratum_bcm_opennsa_bin",
        ":stratum_bcm_opennsa_lib",
    ],
)

pkg_deb(
    name = "stratum_bcm_opennsa_deb",
    architecture = "amd64",
    conflicts = [
        "stratum-bcm-sdk6",
        "stratum-bcm-sdklt",
    ],
    data = ":statum_bcm_opennsa_all",
    depends = [
        "kmod",
        "libboost-thread1.62.0",
        "libedit2",
        "libjudydebian1",
        "libssl1.1",
    ],
    description = "Stratum for Broadcom switches using OpenNSA.",
    homepage = "https://stratumproject.org/",
    maintainer = "The Stratum Project",
    package = "stratum-bcm-opennsa",
    version = "0.0.1",
)

pkg_deb(
    name = "stratum_bcm_sdklt_deb",
    architecture = "amd64",
    conflicts = [
        "stratum-bcm-opennsa",
        "stratum-bcm-sdk6",
    ],
    data = ":statum_bcm_sdklt_all",
    depends = [
        "kmod",
        "libboost-thread1.62.0",
        "libedit2",
        "libjudydebian1",
        "libssl1.1",
        "libyaml-0-2",
    ],
    description = "Stratum for Broadcom switches using SDKLT.",
    homepage = "https://stratumproject.org/",
    maintainer = "The Stratum Project",
    package = "stratum-bcm-sdklt",
    version = "0.0.1",
)

container_image(
    name = "stratum_bcm_opennsa_docker_with_deb",
    base = "@minideb//image",
    files = [":stratum_bcm_opennsa_deb.deb"],
)

container_image(
    name = "stratum_bcm_sdklt_docker_with_deb",
    base = "@minideb//image",
    files = [":stratum_bcm_sdklt_deb.deb"],
)

container_run_and_commit(
    name = "stratum_bcm_opennsa_docker_installed_deb",
    commands = [
        "install_packages /stratum_bcm_opennsa_deb.deb",
        "rm /stratum_bcm_opennsa_deb.deb",
    ],
    image = ":stratum_bcm_opennsa_docker_with_deb.tar",
)

container_run_and_commit(
    name = "stratum_bcm_sdklt_docker_installed_deb",
    commands = [
        "install_packages /stratum_bcm_sdklt_deb.deb",
        "rm /stratum_bcm_sdklt_deb.deb",
    ],
    image = ":stratum_bcm_sdklt_docker_with_deb.tar",
)

container_image(
    name = "stratum_bcm_opennsa_docker",
    base = ":stratum_bcm_opennsa_docker_installed_deb",
    entrypoint = ["start-stratum.sh"],
    labels = {
        "maintainer": "Stratum dev <stratum-dev@lists.stratumproject.org>",
        "description": "This is the Stratum runtime Docker image for Broadcom switches",
        "stratum-target": "stratum_bcm_opennsa",
        "build-timestamp": "{BUILD_TIMESTAMP}",
        "build-machine": "{BUILD_HOST}",
        "org.opencontainers.image.source": "todo",  # TODO(max): extend with --workspace_status_command
        "org.opencontainers.image.version": "todo",
        "org.opencontainers.image.revision": "todo",
    },
    ports = [
        "9339",
        "9559",
        "28000",
    ],
    repository = "stratumproject",
    stamp = True,
    workdir = "/var/run/stratum",
)

container_image(
    name = "stratum_bcm_sdklt_docker",
    base = ":stratum_bcm_sdklt_docker_installed_deb",
    entrypoint = ["start-stratum.sh"],
    labels = {
        "maintainer": "Stratum dev <stratum-dev@lists.stratumproject.org>",
        "description": "This is the Stratum runtime Docker image for Broadcom switches",
        "stratum-target": "stratum_bcm_sdklt",
        "build-timestamp": "{BUILD_TIMESTAMP}",
        "build-machine": "{BUILD_HOST}",
        "org.opencontainers.image.source": "todo",  # TODO(max): extend with --workspace_status_command
        "org.opencontainers.image.version": "todo",
        "org.opencontainers.image.revision": "todo",
    },
    ports = [
        "9339",
        "9559",
        "28000",
    ],
    repository = "stratumproject",
    stamp = True,
    workdir = "/var/run/stratum",
)

container_push(
    name = "push_stratum_bcm_opennsa_docker",
    format = "Docker",
    image = ":stratum_bcm_opennsa_docker",
    registry = "index.docker.io",
    repository = "stratumproject/stratum-bcm",
    tag = "opennsa",
)

container_push(
    name = "push_stratum_bcm_sdklt_docker",
    format = "Docker",
    image = ":stratum_bcm_sdklt_docker",
    registry = "index.docker.io",
    repository = "stratumproject/stratum-bcm",
    tag = "sdklt",
)
