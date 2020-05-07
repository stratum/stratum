# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This image is not optimized for size, we can figure this out when Stratum
# builds

FROM stratumproject/build:build

ARG USER_NAME=stratum
ARG USER_ID=1000

RUN useradd -ms /bin/bash -l -u $USER_ID $USER_NAME && \
    adduser $USER_NAME sudo && \
    echo "%sudo ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers

WORKDIR /stratum

# TODO: Enable after open source launch and when the code is available in the container
# RUN xargs -a .circleci/build-targets.txt bazel build
