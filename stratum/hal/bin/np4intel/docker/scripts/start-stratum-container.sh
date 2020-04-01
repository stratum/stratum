#!/bin/bash
#
# Copyright 2020-present Open Networking Foundation
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

CONFIG_DIR=${CONFIG_DIR:-/root}
LOG_DIR=${LOG_DIR:-/var/log}
DOCKER_IMAGE=${DOCKER_IMAGE:-stratumproject/stratum-np4intel}
ENTRYPOINT="/stratum-entrypoint.sh"
CMD=""

#
# This script is used to start the stratum container
#

print_help() {
cat << EOF

The script starts the containerized version of Stratum for NP4 Intel based devices.

Usage: $0
    [--debug]                       Start the debug stratum binary
    [--bash]                        Run a bash shell in the container

Example:
    $0

EOF
}


while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
        -h|--help)
        print_help
        exit 0
        ;;
    --debug)
        CMD="--debug"
        shift
        ;;
    --bash)
        ENTRYPOINT="/bin/bash"
        shift
        ;;
    "--")
        shift
        break
        ;;
    *)  # unknown option
        print_help
        exit 1
        ;;
    esac
done

docker run -it --privileged \
    -v /dev:/dev -v /sys:/sys  \
    -p 28000:28000 \
    -v $CONFIG_DIR:/stratum_configs \
    -v $LOG_DIR:/stratum_logs \
    --entrypoint=$ENTRYPOINT \
    $DOCKER_IMAGE $CMD 
