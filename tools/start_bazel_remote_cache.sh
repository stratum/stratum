#!/usr/bin/env bash
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -ex -u pipefail

THIS_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
CACHE_DATA_DIR="$THIS_DIR/bazel-remote-cache-data"
HTPASSWD_FILE="$THIS_DIR/htpasswd"
BAZEL_CACHE_USER="circleci"
BAZEL_CACHE_SIZE_GB=10

if [[ ! -f $HTPASSWD_FILE ]]; then
    BAZEL_CACHE_PASSWORD=$(head -c 20 /dev/urandom | shasum -a 256 | head -c 16)
    htpasswd -c -i "$HTPASSWD_FILE" "$BAZEL_CACHE_USER" <<< "$BAZEL_CACHE_PASSWORD"
    echo "Created new passwd file at $HTPASSWD_FILE with user $BAZEL_CACHE_USER" \
         "and password $BAZEL_CACHE_PASSWORD"
fi

docker run -u 0:0 -d --restart=on-failure --name bazel-remote-cache \
    -v "$CACHE_DATA_DIR":/data \
    -v "$HTPASSWD_FILE":/etc/bazel-remote/htpasswd \
    -p 8080:8080 -p 9092:9092 \
    buchgr/bazel-remote-cache \
        --max_size="$BAZEL_CACHE_SIZE_GB" \
	      --htpasswd_file /etc/bazel-remote/htpasswd \
	      --allow_unauthenticated_reads

echo "Started Bazel remote cache."
     "build --remote_cache=http://$BAZEL_CACHE_USER:<pw>@<ip>:8080"
echo "To stop: docker container rm -f bazel-remote-cache"
