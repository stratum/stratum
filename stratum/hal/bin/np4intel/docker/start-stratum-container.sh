#!/bin/bash
#
# Copyright 2019-present Dell EMC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#
set -e
set -x

DOCKERFILE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CONFIG_DIR=${CONFIG_DIR:-${DOCKERFILE_DIR}/configs}
LOG_DIR=${LOG_DIR:-${DOCKERFILE_DIR}/logs}
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

docker run -it --rm --privileged \
    -v /dev:/dev -v /sys:/sys  \
    -p 28000:28000 \
    -p 9339:9339 \
    -p 9559:9559 \
    -v $CONFIG_DIR:/stratum_configs \
    -v $LOG_DIR:/stratum_logs \
    --entrypoint=$ENTRYPOINT \
    $DOCKER_IMAGE $CMD
