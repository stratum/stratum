#!/bin/bash
#
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

# TODO(max): add more files to the checklist over time.
KNOWN_FILES=(
  "README.md"
  "CONTRIBUTING.md"
)

# For all available rules, see:
# https://github.com/markdownlint/markdownlint/blob/master/docs/RULES.md

docker run --rm -v $(pwd):$(pwd) -w $(pwd) markdownlint/markdownlint \
  -v --rules MD013 \
  ${KNOWN_FILES[*]}
