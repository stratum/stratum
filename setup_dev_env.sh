#!/usr/bin/env bash

PULL_DOCKER=NO
MOUNT_SSH=NO
BAZEL_CACHE=/home/$USER/.cache

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
    [--mount-ssh]                   mount the ~/.ssh directory into the docker image
    [--bazel-cache <path>]          mount the provided directory into the docker image and use it as the Bazel cache;
                                    default is ~/.cache
    [--git-name <name>]             use the provided name for git commits
    [--git-email <email>]           use the provided email for git commits
    [--git-editor <editor command>] use the provided editor for git
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
        BAZEL_CACHE="$1"
        shift
        shift
        ;;
    --git-name)
        GIT_NAME="$1"
        shift
        shift
        ;;
    --git-email)
        GIT_EMAIL="$1"
        shift
        shift
        ;;
    --git-editor)
        GIT_EDITOR="$1"
        shift
        shift
        ;;
    *)  # unknown option
        print_help
        exit 1
        ;;
    esac
done

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
IMAGE_NAME=stratum-dev

DOCKER_BUILD_OPTIONS="-t $IMAGE_NAME"
if [ "$PULL_DOCKER" == YES ]; then
    DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --pull"
fi
DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --build-arg USER_NAME=$USER --build-arg USER_ID=$UID"
if [ ! -z "$GIT_NAME" ]; then
    DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --build-arg GIT_GLOBAL_NAME='$GIT_NAME'"
fi
if [ ! -z "$GIT_EMAIL" ]; then
    DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --build-arg GIT_GLOBAL_EMAIL='$GIT_EMAIL'"
fi
if [ ! -z "$GIT_EDITOR" ]; then
    DOCKER_BUILD_OPTIONS="$DOCKER_BUILD_OPTIONS --build-arg GIT_GLOBAL_EDITOR='$GIT_EDITOR'"
fi
docker build $DOCKER_BUILD_OPTIONS -f $THIS_DIR/Dockerfile.dev $THIS_DIR

DOCKER_RUN_OPTIONS="--rm -v $THIS_DIR:/stratum"
if [ "$MOUNT_SSH" == YES ]; then
    DOCKER_RUN_OPTIONS="$DOCKER_RUN_OPTIONS -v ~/.ssh:/home/$USER/.ssh"
fi
DOCKER_RUN_OPTIONS="$DOCKER_RUN_OPTIONS -v $BAZEL_CACHE:$BAZEL_CACHE"
docker run $DOCKER_RUN_OPTIONS -w /stratum --user $USER -ti $IMAGE_NAME bash
