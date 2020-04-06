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

if [[ $EUID -eq 0 ]]; then
   echo "This script should not be run as root, run it as the user who owns the Stratum source directory"
   exit 1
fi

PULL_DOCKER=NO
MOUNT_SSH=NO
BAZEL_CACHE=$HOME/.cache

print_help() {
cat << EOF
Builds a docker image using Dockerfile.builder and to be used as the base
image used in the setup_ubuntu_dev_env.sh.  The host ssh keys can also be 
mounted in the docker to facilitate git usage. The docker image will take some
time to build the first time this script is run.

Usage: $0
    [--pull]                        pull the latest debian base image
    [--mount-ssh]                   mount the HOME/.ssh directory into the docker image
    [--bazel-cache <path>]          mount the provided directory into the docker image and use it as the Bazel cache;
                                    default is HOME/.cache
    [--git-name <name>]             use the provided name for git commits
    [--git-email <email>]           use the provided email for git commits
    [--git-editor <editor command>] use the provided editor for git
    [-- [Docker options]]           additional Docker options for running the container
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
    --pull)
        PULL_DOCKER=YES
        shift
        ;;
    --mount-ssh)
        MOUNT_SSH=YES
        shift
        ;;
    --bazel-cache)
        BAZEL_CACHE="$2"
        shift
        shift
        ;;
    --git-name)
        GIT_NAME="$2"
        shift
        shift
        ;;
    --git-email)
        GIT_EMAIL="$2"
        shift
        shift
        ;;
    --git-editor)
        GIT_EDITOR="$2"
        shift
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


THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DOCKERFILE_DIR=$THIS_DIR/stratum/hal/bin/np4intel/docker
JOBS=${JOBS:-4}
IMAGE_NAME=stratumproject/stratum-np4intel-builder

DOCKER_BUILD_OPTIONS="-t $IMAGE_NAME"
if [ "$PULL_DOCKER" == YES ]; then
    DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --pull"
fi

# Pass in the deploy key for the NP4 repo
SSH_DEPLOY_KEY=$(cat ~/.ssh/deploy)

docker build $DOCKER_BUILD_OPTIONS \
    --build-arg JOBS="$JOBS" \
    --build-arg SSH_DEPLOY_KEY="$SSH_DEPLOY_KEY" \
    -f $DOCKERFILE_DIR/Dockerfile.builder $THIS_DIR

ERR=$?
if [ $ERR -ne 0 ]; then
    >&2 echo "ERROR: Error while building stratum builder image"
    exit $ERR
fi

