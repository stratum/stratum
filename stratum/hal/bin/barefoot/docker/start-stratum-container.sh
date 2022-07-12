#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

LOG_DIR=${LOG_DIR:-/var/log}
SDE_VERSION=${SDE_VERSION:-9.7.2}
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-bfrt}
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG:-latest-$SDE_VERSION}

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

# Set Docker network options.
# On tofino-model and certain switches we run Stratum directly on the host
# network. The BSP on Wedge devices needs access to the usb0 interface.
if [[ "$PLATFORM" == 'barefoot-tofino-model' ]] || \
   [[ "$PLATFORM" == "x86-64-accton-wedge100bf-32x-r0" ]] || \
   [[ "$PLATFORM" == "x86-64-accton-wedge100bf-32qs-r0" ]] || \
   [[ "$PLATFORM" == "x86-64-accton-wedge100bf-65x-r0" ]]; then
    DOCKER_NET_OPTS="--network host "
else
    DOCKER_NET_OPTS="-p 9339:9339 "
    DOCKER_NET_OPTS+="-p 9559:9559 "
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

# Start Stratum.
set -x
docker run -it --rm --privileged \
    -v /dev:/dev -v /sys:/sys  \
    -v /lib/modules/$(uname -r):/lib/modules/$(uname -r) \
    --env PLATFORM=$PLATFORM \
    $DOCKER_NET_OPTS \
    $ONLP_MOUNT \
    $CHASSIS_CONFIG_MOUNT \
    -v $LOG_DIR:/var/log/stratum \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG \
    $@
