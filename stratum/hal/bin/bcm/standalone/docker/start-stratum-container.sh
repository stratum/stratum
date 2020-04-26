#!/bin/bash
#
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#

set -ex

ONLP_MOUNT=$(ls /lib/**/libonlp* | awk '{print "-v " $1 ":" $1 " " }')

CONFIG_DIR=${CONFIG_DIR:-/root/stratum_configs}
LOG_DIR=${LOG_DIR:-/var/log/stratum}
KERNEL_VERSION=$(uname -r)
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-bcm}
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG:-latest}

# --shm-size: https://bugs.freedesktop.org/show_bug.cgi?id=100432
# --cap-add: to create the packetIO interface (bcm-0-0)
# --network host: to have access to the packetIO interface
docker run -it --privileged --cap-add ALL --shm-size=512m --network host \
    -v /dev:/dev -v /sys:/sys -v /run:/run \
    -v /lib/modules/$(uname -r):/lib/modules/$(uname -r) \
    $ONLP_MOUNT \
    -v /lib/platform-config:/lib/platform-config \
    -v /etc/onl:/etc/onl \
    -v $CONFIG_DIR:/etc/stratum/stratum_configs \
    -v $LOG_DIR:/var/log/stratum \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG
