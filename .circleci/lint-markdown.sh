#!/bin/bash
#
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

docker run --rm -v $(pwd):$(pwd) -w $(pwd) markdownlint/markdownlint -v --rules MD013 .
