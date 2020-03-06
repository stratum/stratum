#
# Copyright 2019-present Open Networking Foundation
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

FROM stratumproject/build:build as builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    python \
    python-yaml \
    python-pip \
    libssl-dev \
    libelf-dev && \
    rm -rf /var/lib/apt/lists/*

ARG SDE_TAR
ARG KERNEL_HEADERS_TAR
# Copy SDE and Linux headers tarball
COPY $SDE_TAR /stratum/
COPY $KERNEL_HEADERS_TAR /stratum/
COPY /stratum/stratum/hal/bin/barefoot/docker/build-kdrv.sh /build-kdrv.sh

ENV SDE /bf-sde
ENV SDE_INSTALL /$SDE/install
RUN mkdir $SDE && tar xf /stratum/$SDE_TAR -C $SDE --strip-components 1

WORKDIR $SDE/p4studio_build

ARG JOBS=4
# Remove Thrift dependency from the profile (for SDE <= 8.9.x)
RUN sed -i.bak '/package_dependencies/d; /thrift/d' profiles/stratum_profile.yaml
RUN ./p4studio_build.py -up stratum_profile -wk -j$JOBS -shc && \
    rm -rf /var/lib/apt/lists/*

# Build Barefoot Tofino kernel module
ENV KERNEL_HEADERS_PATH=/usr/src/kernel-headers
ENV KDRV_DIR=/bf-sde/pkgsrc/bf-drivers/kdrv/bf_kdrv
RUN /build-kdrv.sh

# Prepare all SDE libraries
ENV OUTPUT_BASE /output/usr/local
RUN mkdir -p $OUTPUT_BASE/lib/modules && \
    cp -d $SDE_INSTALL/lib/*.so* $OUTPUT_BASE/lib/ && \
    mkdir -p $OUTPUT_BASE/share/stratum && \
    cp -r $SDE_INSTALL/share/microp_fw $OUTPUT_BASE/share/ && \
    cp -r $SDE_INSTALL/share/bfsys/ $OUTPUT_BASE/share/ && \
    cp -r $SDE_INSTALL/share/tofino_sds_fw $OUTPUT_BASE/share/

# Strip symbols from all .so files
RUN strip --strip-all $OUTPUT_BASE/lib/*.so*

# Remove SDE and Linux headers tarball
RUN rm -r /stratum/*
