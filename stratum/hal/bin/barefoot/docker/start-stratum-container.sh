#!/bin/bash

set -e
set -x

ONLP_MOUNT=$(ls /lib/**/libonlp* | awk '{print "-v " $1 ":" $1 " " }')

CONFIG_DIR=${CONFIG_DIR:-/root}
LOG_DIR=${LOG_DIR:-/var/log}
SDE_VERSION=${SDE_VERSION:-8.9.1}
KERNEL_VERSION=$(uname -r)
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-bf}
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG:-$SDE_VERSION-$KERNEL_VERSION}

docker run -it --privileged \
    -v /dev:/dev -v /sys:/sys  \
    -v /lib/modules/$(uname -r):/lib/modules/$(uname -r) \
    $ONLP_MOUNT \
    -v /lib/platform-config:/lib/platform-config \
    -v /etc/onl:/etc/onl \
    -p 28000:28000 \
    -v $CONFIG_DIR:/stratum_configs \
    -v $LOG_DIR:/stratum_logs \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG
