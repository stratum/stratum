#!/bin/bash
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

cpplint --recursive --exclude=stratum/hal/lib/bcm/sdklt/bcm_sdk_wrapper.cc stratum
