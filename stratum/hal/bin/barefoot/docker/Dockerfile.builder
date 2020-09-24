#
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
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
COPY $SDE_TAR /stratum/

ENV SDE /bf-sde
ENV SDE_INSTALL $SDE/install
RUN mkdir $SDE && tar xf /stratum/$SDE_TAR -C $SDE --strip-components 1

# Install an older version of pyresistent before running the P4 studio
# since the pip will try to install newer version of it when pip install
# the jsonschema library. And the new version of pyresistent(0.17.x) requires
# Python >= 3.5
# TODO: Remove this once we moved to Python3
RUN pip install pyrsistent==0.14.0

WORKDIR $SDE/p4studio_build

ARG JOBS=4
# Remove Thrift dependency from the profile (for SDE <= 8.9.x)
RUN sed -i.bak '/package_dependencies/d; /thrift/d' profiles/stratum_profile.yaml
RUN ./p4studio_build.py -up stratum_profile -wk -j$JOBS -shc && \
    rm -rf /var/lib/apt/lists/*

# Strip symbols from all .so files
RUN strip --strip-all $SDE_INSTALL/lib/*.so*

# Remove SDE and Linux headers tarball
RUN rm -r /stratum/*
