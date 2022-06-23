#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

ONLP_MOUNT=$(ls /lib/**/libonlp* | awk '{print "-v " $1 ":" $1 " " }')

LOG_DIR=${LOG_DIR:-/var/log/stratum}
KERNEL_VERSION=$(uname -r)
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-bcm}
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG:-sdklt}
PLATFORM=$(cat /etc/onl/platform)

if [ -n "$CHASSIS_CONFIG" ]; then
    CHASSIS_CONFIG_MOUNT="-v $CHASSIS_CONFIG:/etc/stratum/$PLATFORM/chassis_config.pb.txt"
fi

# --shm-size: https://bugs.freedesktop.org/show_bug.cgi?id=100432
# --cap-add: to create the packetIO interface (bcm-0-0)
# --network host: to have access to the packetIO interface
docker run -it --rm --privileged --cap-add ALL --shm-size=512m --network host \
    -v /dev:/dev -v /sys:/sys -v /run:/run \
    -v /lib/modules/$(uname -r):/lib/modules/$(uname -r) \
    $ONLP_MOUNT \
    -v /lib/platform-config:/lib/platform-config \
    -v /etc/onl:/etc/onl \
    $CHASSIS_CONFIG_MOUNT \
    -v $LOG_DIR:/var/log/stratum \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG \
    $@
