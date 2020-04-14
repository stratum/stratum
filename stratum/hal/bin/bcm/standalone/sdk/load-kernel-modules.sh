#!/bin/bash
#
# Copyright 2020-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
set -ex

# Remove other SDK kernel modules, if present
rmmod linux_ngknet || true
rmmod linux_ngbde || true

# Reinsert SDKLT kernel modules
rmmod linux_bcm_knet || true
rmmod linux_user_bde || true
rmmod linux_kernel_bde || true
insmod linux-kernel-bde.ko && insmod linux-user-bde.ko && insmod linux-bcm-knet.ko
sleep 1

# Setup sym links
mknod /dev/linux-bcm-knet c 122 0 || true
mknod /dev/linux-bcm-net c 123 0 || true
mknod /dev/linux-bcm-diag c 124 0 || true
ln -sf /dev/linux-bcm-diag /dev/linux-bcm-diag-full || true
mknod /dev/linux-uk-proxy c 125 0 || true
mknod /dev/linux-bcm-core c 126 0 || true
mknod /dev/linux-kernel-bde c 127 0 || true
ln -sf  /dev/linux-bcm-core  /dev/linux-user-bde || true
