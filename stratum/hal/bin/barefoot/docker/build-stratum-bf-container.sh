#!/bin/bash

STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 SDE_TAR KERNEL_HEADERS_TAR"
    exit 1
fi

docker build -t stratumproject/stratum-bf \
               --build-arg JOBS=$JOBS \
               --build-arg SDE_TAR=$1 \
               --build-arg KERNEL_HEADERS_TAR=$2 \
               -f $STRATUM_ROOT/stratum/hal/bin/barefoot/docker/Dockerfile $STRATUM_ROOT
