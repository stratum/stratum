#!/bin/bash
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

STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}



if [ -z $TM ]; then
  if [ "$#" -ne 2 ]; then
      echo "Usage: $0 SDE_TAR KERNEL_HEADERS_TAR"
      exit 1
  fi

  docker build -t stratumproject/stratum-bf \
                 --build-arg JOBS=$JOBS \
                 --build-arg SDE_TAR=$1 \
                 --build-arg KERNEL_HEADERS_TAR=$2 \
                 -f $STRATUM_ROOT/stratum/hal/bin/barefoot/docker/Dockerfile $STRATUM_ROOT
else
  if [ "$#" -ne 1 ]; then
      echo "Usage: $0 SDE_TAR"
      exit 1
  fi

  docker build -t stratumproject/stratum-bf \
                 --build-arg JOBS=$JOBS \
                 --build-arg SDE_TAR=$1 \
                 -f $STRATUM_ROOT/stratum/hal/bin/barefoot/docker/Dockerfile.tm $STRATUM_ROOT
fi