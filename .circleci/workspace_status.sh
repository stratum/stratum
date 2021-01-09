#!/bin/bash
#
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This script will be run bazel when building process starts to
# generate key-value information that represents the status of the
# workspace. The output should be like
#
# KEY1 VALUE1
# KEY2 VALUE2
#
# Keys starting with "STABLE_" will go into the stable status file. Others in
# the volatile.
#
# If the script exits with non-zero code, it's considered as a failure
# and the output will be discarded.
set -e -o pipefail

# The upstream git URL
git_url=$(git config --get remote.origin.url)
echo "GIT_URL ${git_url}"
echo "STABLE_GIT_URL ${git_url}"

# The git commit checksum, with "-dirty" if modified
git_sha=$(git describe --tags --match XXXXXXX --always --abbrev=40 --dirty)
echo "GIT_SHA ${git_sha}"
echo "STABLE_GIT_SHA ${git_sha}"

# Tag name, or GIT_SHA if not on a tag
git_ref=$(git describe --tags --no-match --always --abbrev=40 --dirty | sed -E 's/^.*-g([0-9a-f]{40}-?.*)$/\1/')
echo "GIT_REF ${git_ref}"
echo "STABLE_GIT_REF ${git_ref}"

echo "STABLE_TS" $(date +%FT%T%z)

echo "BUILD_SCM_REVISION" $(git rev-parse HEAD)
