#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e
set -x

if [ -n "$PLATFORM" ]; then
    # Use specific platorm port map
    ONLP_ARG="--env WITH_ONLP=false \
              --env PLATFORM=$PLATFORM"
elif [ -d "/etc/onl" ]; then
    # Use ONLP to find platform and it's library
    ONLP_ARG=$(ls /lib/**/libonlp* | awk '{print "-v " $1 ":" $1 " " }')
    ONLP_ARG="$ONLP_ARG \
              -v /lib/platform-config:/lib/platform-config \
              -v /etc/onl:/etc/onl"
else
    # Use default platform port map
    ONLP_ARG="--env WITH_ONLP=false \
              --env PLATFORM=x86-64-accton-wedge100bf-32x-r0"
fi

CONFIG_DIR=${CONFIG_DIR:-/root}
LOG_DIR=${LOG_DIR:-/var/log}
SDE_VERSION=${SDE_VERSION:-9.0.0}
KERNEL_VERSION=$(uname -r)
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-bf}
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG:-$SDE_VERSION-$KERNEL_VERSION}

docker run -it --privileged \
    -v /dev:/dev -v /sys:/sys  \
    -v /lib/modules/$(uname -r):/lib/modules/$(uname -r) \
    $ONLP_ARG \
    -p 28000:28000 \
    -p 9339:9339 \
    -v $CONFIG_DIR:/stratum_configs \
    -v $LOG_DIR:/stratum_logs \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG
