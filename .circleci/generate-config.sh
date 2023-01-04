#!/bin/bash
# Copyright 2022-present Intel
# SPDX-License-Identifier: Apache-2.0

STRATUM_ROOT=$(cd $(dirname ${BASH_SOURCE[0]})/..; pwd)
STRATUM_BUILDER_IMAGE="stratumproject/build:build"

if [[ "$CIRCLE_BRANCH" != "main" ]] && [[ -n $DOCKER_LOGIN ]]; then
    DOCKERFILE_SHA=$(sha256sum "$STRATUM_ROOT/Dockerfile.build" | awk '{print $1}')
    STRATUM_BUILDER_IMAGE="stratumproject/build-ci:$DOCKERFILE_SHA"
fi

DATE_YY_MM_DD=$(date "+%y.%m.%d")

sed -e "s#STRATUM_BUILDER_IMAGE#$STRATUM_BUILDER_IMAGE#g" \
    -e "s#DATE_YY_MM_DD#$DATE_YY_MM_DD#g" \
  "$STRATUM_ROOT/.circleci/config_template.yml"
