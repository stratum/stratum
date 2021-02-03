#!/bin/bash
# Copyright 2021-present Extreme Networks, Inc.
# SPDX-License-Identifier: Apache-2.0

# Run stratum unit tests inside the stratum dev container.
# Run from /stratum.

TEST_CMD="xargs -a .circleci/test-targets.txt bazel test"
# --flaky_test_attempts to retry transient admin_service_test failures
TEST_CMD="$TEST_CMD --flaky_test_attempts 5"
eval $TEST_CMD
