#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

veth_setup.sh ||
( echo "Failed to set up interfaces. You need to run in priveledged mode.";
  exit 1 )

dma_setup.sh
tofino-model --p4-target-config /usr/share/stratum/tofino_skip_p4.conf $@
