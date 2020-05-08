#!/bin/bash
# Copyright 2020 Open Networking Foundation
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
Builds a docker image using Dockerfile.builder and to be used as the base
image used in the setup_ubuntu_dev_env.sh.  The host ssh keys can also be
mounted in the docker to facilitate git usage. The docker image will take some
time to build the first time this script is run.

Usage: $0 [options] -- NP4_TAR
    [--pull]                        pull the latest debian base image
    [--mount-ssh]                   mount the HOME/.ssh directory into the docker image
    [--bazel-cache <path>]          mount the provided directory into the docker image and use it as the Bazel cache;
                                    default is HOME/.cache
    [--git-name <name>]             use the provided name for git commits
    [--git-email <email>]           use the provided email for git commits
    [--git-editor <editor command>] use the provided editor for git
    [-- [Docker options]]           additional Docker options for running the container

Example:
    $0 -- ~/np4_intel_4_7_1-1.tgz

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

# We need at least 1 arg
if [ "$#" -eq 0 ]; then
    print_help
    exit 1
fi

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DOCKERFILE_DIR=$THIS_DIR/stratum/hal/bin/np4intel/docker
JOBS=${JOBS:-4}
IMAGE_NAME=stratumproject/stratum-np4intel-builder

BUILD_ARGS=""
if [ "$PULL_DOCKER" == YES ]; then
    BUILD_ARGS="$BUILD_ARGS --pull"
fi
BUILD_ARGS="$BUILD_ARGS --build-arg JOBS=$JOBS"

# Grab the NP4 tarball
echo """Copying NP4 tarball to $DOCKERFILE_DIR/
NOTE: Copied tarballs will be DELETED after the build"""

if [ -n "$1" ]; then
    NP4_TAR=$(basename $1)
    BUILD_ARGS="$BUILD_ARGS --build-arg NP4_TAR=$NP4_TAR"
    cp -f $1 $THIS_DIR
fi

# Build the container
docker build -t $IMAGE_NAME $BUILD_ARGS \
    -f $DOCKERFILE_DIR/Dockerfile.builder $THIS_DIR

ERR=$?
if [ $ERR -ne 0 ]; then
    >&2 echo "ERROR: Error while building stratum builder image"
    exit $ERR
fi

# Remove copied tarballs
if [ -f "$THIS_DIR/$NP4_TAR" ]; then
    rm -f $THIS_DIR/$NP4_TAR
fi

