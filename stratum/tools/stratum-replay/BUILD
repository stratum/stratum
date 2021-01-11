# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
)
load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_image",
    "container_push",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_binary(
    name = "stratum-replay",
    srcs = ["stratum_replay.cc"],
    arches = HOST_ARCHES,
    deps = [
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/p4:forwarding_pipeline_configs_cc_proto",
        "//stratum/hal/lib/p4:utils",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_gflags_gflags//:gflags",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
    ],
)

container_image(
    name = "stratum-replay_docker",
    base = "@minideb//image",
    cmd = ["-help"],
    entrypoint = ["/stratum-replay"],
    files = [":stratum-replay"],
    labels = {
        "maintainer": "Stratum dev <stratum-dev@lists.stratumproject.org>",
        "description": "Docker-based distribution of the Stratum-bf pipeline builder",
        "build-timestamp": "{BUILD_TIMESTAMP}",
        "build-machine": "{BUILD_HOST}",
        "org.opencontainers.image.source": "todo",  # TODO(max): extend with --workspace_status_command
        "org.opencontainers.image.version": "todo",
        "org.opencontainers.image.revision": "todo",
    },
    repository = "stratumproject",
    stamp = True,
)

container_push(
    name = "push_stratum-replay_docker",
    format = "Docker",
    image = ":stratum-replay_docker",
    registry = "index.docker.io",
    repository = "stratumproject/stratum-replay",
    tag = "latest",
)
