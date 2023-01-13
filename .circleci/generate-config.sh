#!/bin/bash
# Copyright 2022-present Intel Corporation
# SPDX-License-Identifier: Apache-2.0

STRATUM_ROOT=$(cd $(dirname ${BASH_SOURCE[0]})/..; pwd)
STRATUM_BUILDER_IMAGE="stratumproject/build:build"

if [[ "$CIRCLE_BRANCH" != "main" ]] && [[ -n $DOCKER_LOGIN ]]; then
    DOCKERFILE_SHA=$(sha256sum "$STRATUM_ROOT/Dockerfile.build" | awk '{print $1}')
    STRATUM_BUILDER_IMAGE="stratumproject/build-ci:$DOCKERFILE_SHA"
fi

sed "s#STRATUM_BUILDER_IMAGE#$STRATUM_BUILDER_IMAGE#g" "$STRATUM_ROOT/.circleci/config_template.yml"
