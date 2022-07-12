#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

LOG_DIR=${LOG_DIR:-/var/log/stratum}
KERNEL_VERSION=$(uname -r)
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-bcm}
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG:-sdklt}

# Try to load the platform string if not already set.
if [[ -z "$PLATFORM" ]] && [[ -f "/etc/onl/platform" ]]; then
    PLATFORM=$(cat /etc/onl/platform)
elif [[ -z "$PLATFORM" ]] && [[ -f "/etc/sonic/sonic-environment" ]]; then
    PLATFORM=$(source /etc/sonic/sonic-environment; echo "$PLATFORM" | sed 's/_/-/g')
    echo "Stopping SONiC services..."
    sudo systemctl stop sonic.target
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

if [ -n "$CHASSIS_CONFIG" ]; then
    CHASSIS_CONFIG_MOUNT="-v $CHASSIS_CONFIG:/etc/stratum/$PLATFORM/chassis_config.pb.txt"
fi

# --shm-size: https://bugs.freedesktop.org/show_bug.cgi?id=100432
# --cap-add: to create the packetIO interface (bcm-0-0)
# --network host: to have access to the packetIO interface
docker run -it --rm --privileged --cap-add ALL --shm-size=512m --network host \
    -v /dev:/dev -v /sys:/sys -v /run:/run \
    -v /lib/modules/$(uname -r):/lib/modules/$(uname -r) \
    -v /lib/platform-config:/lib/platform-config \
    --env PLATFORM=$PLATFORM \
    $ONLP_MOUNT \
    $CHASSIS_CONFIG_MOUNT \
    -v $LOG_DIR:/var/log/stratum \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG \
    $@
