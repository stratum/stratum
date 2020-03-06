#!/bin/bash
#
# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
set -e
set -x

PLATFORM=${PLATFORM:x86-64-accton-wedge100bf-32x-r0}

if [ -d "/etc/onl" ]; then
    ONLP_ARG=$(ls /lib/**/libonlp* | awk '{print "-v " $1 ":" $1 " " }')
    ONLP_ARG="$ONLP_ARG \
              -v /lib/platform-config:/lib/platform-config \
              -v /etc/onl:/etc/onl"
else
    ONLP_ARG="--env WITH_ONLP=false \
              --env PLATFORM=$PLATFORM"
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
    -v $CONFIG_DIR:/stratum_configs \
    -v $LOG_DIR:/stratum_logs \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG
