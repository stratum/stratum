#!/bin/bash
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This script will be run bazel when building process starts to
# generate key-value information that represents the status of the
# workspace. The output should be like
#
# KEY1 VALUE1
# KEY2 VALUE2
#
# Keys starting with "STABLE_" will go into the stable status file. All others
# into the volatile file.
# See: $(bazel info output_path)/stable-status.txt
# and: $(bazel info output_path)/volatile-status.txt
#
# If the script exits with non-zero code, it's considered as a failure
# and the output will be discarded.
set -e -o pipefail

# If not building from a git repository, we can't generate any stamping info.
if [ ! -d ".git" ]; then
  exit 0
fi

# The upstream git URL
git_url=$(git config --get remote.origin.url)
echo "GIT_URL ${git_url}"

# The git commit checksum, with "-dirty" if modified
git_sha=$(git describe --tags --match XXXXXXX --always --abbrev=40 --dirty)
echo "GIT_SHA ${git_sha}"

# Tag name, or GIT_SHA if not on a tag
git_ref=$(git describe --tags --no-match --always --abbrev=40 --dirty | sed -E 's/^.*-g([0-9a-f]{40}-?.*)$/\1/')
echo "GIT_REF ${git_ref}"

# Plain git revision for linkstamping.
echo "BUILD_SCM_REVISION" $(git rev-parse HEAD)

# Git tree status for linkstamping.
if [[ -n $(git status --porcelain) ]];
then
    tree_status="Modified"
else
    tree_status="Clean"
fi
echo "BUILD_SCM_STATUS ${tree_status}"
