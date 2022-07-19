#!/bin/bash
# Copyright 2022-present Intel
# SPDX-License-Identifier: Apache-2.0
STRATUM_ROOT=$(cd $(dirname ${BASH_SOURCE[0]})/..; pwd)
STRATUM_BUILDER_IMAGE="stratumproject/build:build"

DIFF_FILE=$(mktemp)
git diff "$BASE_REVISION" HEAD "$STRATUM_ROOT/Dockerfile.build" > "$DIFF_FILE"

if [[ -s "$DIFF_FILE" ]]; then
    # CIRCLE_PULL_REQUEST is the link of the pull request which will be
    # unique for each PR.
    PR_SHA=$(echo $CIRCLE_PULL_REQUEST | sha256sum | awk '{print $1}')
    STRATUM_BUILDER_IMAGE="stratumproject/build-ci:$PR_SHA"
fi            

sed "s#STRATUM_BUILDER_IMAGE#$STRATUM_BUILDER_IMAGE#g" "$STRATUM_ROOT/.circleci/config_template.yml"
