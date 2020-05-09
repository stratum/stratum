#!/usr/bin/env bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

if [[ $EUID -eq 0 ]]; then
   echo "This script should not be run as root, run it as the user who owns the Stratum source directory"
   exit 1
fi

PULL_DOCKER=NO
MOUNT_SSH=NO
BAZEL_CACHE=$HOME/.cache

print_help() {
cat << EOF
Builds a docker image using Dockerfile.dev and runs a bash session in it. It is
a convenient environment to do Stratum development. The docker image includes
the Bazel build system, git and popular Linux text editors. This Stratum source
directory will be mounted in the docker image. A local cache directory can be
provided to the running docker image so that restarting the docker does not
trigger a complete rebuild of Stratum. The host ssh keys can also be mounted in
the docker to facilitate git usage. The docker image will take some time to
build the first time this script is run.

Usage: $0
    [--pull]                        pull the latest debian base image
    [--mount-ssh]                   mount the HOME/.ssh directory into the docker image
    [--bazel-cache <path>]          mount the provided directory into the docker image and use it as the Bazel cache;
                                    default is HOME/.cache
    [--git-name <name>]             use the provided name for git commits
    [--git-email <email>]           use the provided email for git commits
    [--git-editor <editor command>] use the provided editor for git
    [--np4-intel]                   create NP4 Intel build environment
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
    --np4-intel)
        NP4_INTEL=YES
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
if [ "$NP4_INTEL" == YES ]; then
    IMAGE_NAME=stratumproject/stratum-np4intel-dev
    DOCKER_FILE=$THIS_DIR/Dockerfile.np4intel.dev
    BAZEL_CACHE=$HOME/.np4intel_cache
else
    IMAGE_NAME=stratum-dev
    DOCKER_FILE=$THIS_DIR/Dockerfile.dev
fi

DOCKER_BUILD_OPTIONS="-t $IMAGE_NAME"
if [ "$PULL_DOCKER" == YES ]; then
    DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --pull"
fi
DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --build-arg USER_NAME=\"$USER\" --build-arg USER_ID=\"$UID\""
if [ ! -z "$GIT_NAME" ]; then
    DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --build-arg GIT_GLOBAL_NAME=\"$GIT_NAME\""
fi
if [ ! -z "$GIT_EMAIL" ]; then
    DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --build-arg GIT_GLOBAL_EMAIL=\"$GIT_EMAIL\""
fi
if [ ! -z "$GIT_EDITOR" ]; then
    DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --build-arg GIT_GLOBAL_EDITOR=\"$GIT_EDITOR\""
fi
eval docker build $DOCKER_BUILD_OPTIONS -f $DOCKER_FILE $THIS_DIR
ERR=$?
if [ $ERR -ne 0 ]; then
    >&2 echo "ERROR: Error while building dockering development image"
    exit $ERR
fi

DOCKER_RUN_OPTIONS="--rm -v $THIS_DIR:/stratum"
if [ "$MOUNT_SSH" == YES ]; then
    DOCKER_RUN_OPTIONS="$DOCKER_RUN_OPTIONS -v $HOME/.ssh:/home/$USER/.ssh"
fi
DOCKER_RUN_OPTIONS="$DOCKER_RUN_OPTIONS -v $BAZEL_CACHE:/home/$USER/.cache"
if [ -n "$NP4_INSTALL" ]; then
    DOCKER_RUN_OPTIONS="$DOCKER_RUN_OPTIONS -v $NP4_INSTALL:/home/$USER/np4_install"
    DOCKER_RUN_OPTIONS="$DOCKER_RUN_OPTIONS -e NP4_INSTALL=$NP4_INSTALL"
else
    DOCKER_RUN_OPTIONS="$DOCKER_RUN_OPTIONS -e NP4_INSTALL=/usr"
fi
if [ "$NP4_INTEL" == YES ]; then
    DOCKER_RUN_OPTIONS="$DOCKER_RUN_OPTIONS -v /dev/intel-fpga-fme.0:/dev/intel-fpga-fme.0"
fi
DOCKER_RUN_OPTIONS="$DOCKER_RUN_OPTIONS $@"
docker run $DOCKER_RUN_OPTIONS -w /stratum --user $USER -ti $IMAGE_NAME bash
