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

ONLP_MOUNT=$(ls /lib/**/libonlp* | awk '{print "-v " $1 ":" $1 " " }')

CONFIG_DIR=${CONFIG_DIR:-/root/stratum_configs}
LOG_DIR=${LOG_DIR:-/var/log}
KERNEL_VERSION=$(uname -r)
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-bcm}
DOCKER_IMAGE_TAG=${DOCKER_IMAGE_TAG:-latest}

# --shm-size: https://bugs.freedesktop.org/show_bug.cgi?id=100432
# --cap-add: to create the packetIO interface (bcm-0-0)
# --network host: to have access to the packetIO interface
docker run -it --privileged --cap-add ALL --shm-size=512m --network host \
    -v /dev:/dev -v /sys:/sys  \
    -v /lib/modules/$(uname -r):/lib/modules/$(uname -r) \
    $ONLP_MOUNT \
    -v /lib/platform-config:/lib/platform-config \
    -v /etc/onl:/etc/onl \
    -v $CONFIG_DIR:/stratum_configs \
    -v $LOG_DIR:/stratum_logs \
    --entrypoint start-stratum.sh \
    $DOCKER_IMAGE:$DOCKER_IMAGE_TAG
