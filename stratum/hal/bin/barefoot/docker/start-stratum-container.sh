#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e
set -x
PLATFORM_ARGS=""

if [ -n "$PLATFORM" ]; then
    # Use specific platorm port map
    PLATFORM_ARGS="--env PLATFORM=$PLATFORM"
elif [ -d "/etc/onl" ]; then
    # Use ONLP to find platform and its library
    PLATFORM_ARGS=$(ls /lib/**/libonlp* | awk '{print "-v " $1 ":" $1 " " }')
    PLATFORM_ARGS="$PLATFORM_ARGS \
              -v /lib/platform-config:/lib/platform-config \
              -v /etc/onl:/etc/onl"
fi

if [ -n "$FLAG_FILE" ]; then
    FLAG_FILE_MOUNT="-v $FLAG_FILE:/etc/stratum/stratum.flags"
fi

if [ -n "$CHASSIS_CONFIG" ]; then
    CHASSIS_CONFIG_MOUNT="-v $CHASSIS_CONFIG:/etc/stratum/$PLATFORM/chassis_config.pb.txt"
fi

LOG_DIR=${LOG_DIR:-/var/log}
SDE_VERSION=${SDE_VERSION:-9.0.0}
KERNEL_VERSION=$(uname -r)
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-bf}
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG:-$SDE_VERSION-$KERNEL_VERSION}

docker run -it --privileged \
    -v /dev:/dev -v /sys:/sys  \
    -v /lib/modules/$(uname -r):/lib/modules/$(uname -r) \
    $PLATFORM_ARGS \
    -p 28000:28000 \
    -p 9339:9339 \
    $FLAG_FILE_MOUNT \
    $CHASSIS_CONFIG_MOUNT \
    -v $LOG_DIR:/var/log/stratum \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG \
    $@
