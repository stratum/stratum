#!/bin/bash
# Copyright 2022-present Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set -ex

LOG_DIR=${LOG_DIR:-/var/log/stratum}
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-dpdk}
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG:-latest}

PLATFORM="p4-dpdk-software-switch"

DOCKER_NET_OPTS="-p 9339:9339 "
DOCKER_NET_OPTS+="-p 9559:9559 "
# DOCKER_NET_OPTS="--network host "

if [ -n "$CHASSIS_CONFIG" ]; then
    CHASSIS_CONFIG_MOUNT="-v $CHASSIS_CONFIG:/etc/stratum/$PLATFORM/chassis_config.pb.txt"
fi

# Start Stratum.
set -x
docker run -it --rm --privileged \
    -v /dev:/dev -v /sys:/sys  \
    $DOCKER_NET_OPTS \
    $CHASSIS_CONFIG_MOUNT \
    -v $LOG_DIR:/var/log/stratum \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG \
    $@
