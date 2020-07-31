#!/usr/bin/env bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# Start mininet in a screen session so we can attach to its CLI later.
screen -dmS cli -L screen.log python /topo/topo.py

# Print CLI outoput to stdout as container log. Make sure to tail on an existing
# file if screen hasn't created it yet...
touch screen.log
tail -f screen.log
