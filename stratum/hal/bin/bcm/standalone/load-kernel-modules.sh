#!/bin/bash
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

# Remove other SDK kernel modules, if present
rmmod linux_bcm_knet || true
rmmod linux_user_bde || true
rmmod linux_kernel_bde || true

# Reinsert SDKLT kernel modules
rmmod linux_ngknet || true
rmmod linux_ngbde || true
pushd /usr/lib/stratum/
insmod linux_ngbde.ko && insmod linux_ngknet.ko
popd
sleep 1
