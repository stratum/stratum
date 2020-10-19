#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -ex

# Try to load the platform string if not already set.
if [[ -z "$PLATFORM" ]] && [[ -f "/etc/onl/platform" ]]; then
    PLATFORM=$(cat /etc/onl/platform)
elif [[ -z "$PLATFORM" ]]; then
    echo "PLATFORM variable must be set manually on non-ONL switches."
    exit 255
fi

# Mount ONL related directories, if they exist.
if [ -d "/etc/onl" ]; then
    # Use ONLP to find platform and its library
    ONLP_MOUNT=$(ls /lib/**/libonlp* | awk '{print "-v " $1 ":" $1 " " }')
    ONLP_MOUNT="$ONLP_MOUNT \
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

docker run -it --rm --privileged \
    -v /dev:/dev -v /sys:/sys  \
    -v /lib/modules/$(uname -r):/lib/modules/$(uname -r) \
    --env PLATFORM=$PLATFORM \
    $ONLP_MOUNT \
    -p 28000:28000 \
    -p 9339:9339 \
    -p 9559:9559 \
    $FLAG_FILE_MOUNT \
    $CHASSIS_CONFIG_MOUNT \
    -v $LOG_DIR:/var/log/stratum \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG \
    $@
