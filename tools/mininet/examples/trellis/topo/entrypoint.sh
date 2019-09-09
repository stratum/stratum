#!/usr/bin/env bash
#
# Copyright 2018-present Open Networking Foundation
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

# Start mininet in a screen session so we can attach to its CLI later.
screen -dmS cli -L screen.log python /topo/topo.py

# Print CLI outoput to stdout as container log. Make sure to tail on an existing
# file if screen hasn't created it yet...
touch screen.log
tail -f screen.log
