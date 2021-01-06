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
# If the script exits with non-zero code, it's considered as a failure
# and the output will be discarded.

# The code below presents an implementation that works for git repository
git_rev=$(git rev-parse HEAD)
if [[ $? != 0 ]];
then
    exit 1
fi
echo "BUILD_SCM_REVISION ${git_rev}"

# Check whether there are any uncommitted changes
git diff-index --quiet HEAD --
if [[ $? == 0 ]];
then
    tree_status="Clean"
else
    tree_status="Modified"
fi
echo "BUILD_SCM_STATUS ${tree_status}"

# The upstream git URL
git_url=$(git config --get remote.origin.url)
if [[ $? != 0 ]];
then
    exit 1
fi
echo "GIT_URL ${git_url}"

# The git commit checksum, with "-dirty" if modified
git_sha=$(git describe --tags --match XXXXXXX --always --abbrev=40 --dirty)
if [[ $? != 0 ]];
then
    exit 1
fi
echo "GIT_SHA ${git_sha}"

# Tag name, or GIT_SHA if not on a tag
git_ref=$(git describe --tags --no-match --always --abbrev=40 --dirty | sed -E 's/^.*-g([0-9a-f]{40}-?.*)$/\1/')
if [[ $? != 0 ]];
then
    exit 1
fi
echo "GIT_REF ${git_ref}"
