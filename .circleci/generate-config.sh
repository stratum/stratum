#!/bin/bash
# Copyright 2022-present Intel
# SPDX-License-Identifier: Apache-2.0

STRATUM_ROOT=$(cd $(dirname ${BASH_SOURCE[0]})/..; pwd)
STRATUM_BUILDER_IMAGE="stratumproject/build:build"
PR_NUMBER=$(echo "$CIRCLE_PULL_REQUEST" | grep -Eo '([0-9]+)')
BASE_REVISION=$(curl -o- -L "https://api.github.com/repos/stratum/stratum/pulls/$PR_NUMBER" | jq -r .base.ref)
DIFF_FILE=$(mktemp)
git diff "$BASE_REVISION" HEAD "$STRATUM_ROOT/Dockerfile.build" > "$DIFF_FILE"

if [[ -s "$DIFF_FILE" ]]; then
    STRATUM_BUILDER_IMAGE="stratumproject/build-ci:pr-$PR_NUMBER"
fi

sed "s#STRATUM_BUILDER_IMAGE#$STRATUM_BUILDER_IMAGE#g" "$STRATUM_ROOT/.circleci/config_template.yml"
