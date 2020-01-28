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

FROM stratumproject/build:build as builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    python \
    python-yaml \
    python-pip \
    libssl-dev && \
    rm -rf /var/lib/apt/lists/*

# Copy stratum source code, include SDE and Linux headers tarball
ADD . /stratum

ARG SDE_TAR
ENV SDE /bf-sde
ENV SDE_INSTALL /$SDE/install
RUN mkdir $SDE && tar xf /stratum/$SDE_TAR -C $SDE --strip-components 1

WORKDIR $SDE/p4studio_build

ARG JOBS=4
# Remove Thrift dependency from the profile (for SDE <= 8.9.x)
RUN sed -i.bak '/package_dependencies/d; /thrift/d' profiles/stratum_profile.yaml
RUN ./p4studio_build.py -up stratum_profile -wk -j$JOBS -shc && \
    rm -rf /var/lib/apt/lists/*

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

# Prepare the stratum_bf
ENV BF_SDE_INSTALL $SDE_INSTALL
WORKDIR /stratum
RUN bazel build //stratum/hal/bin/barefoot:stratum_bf \
    --define sde_ver=$(cat $SDE_INSTALL/share/VERSION) \
    --define profile=phal_sim

ENV STRATUM_BF_BASE stratum/hal/bin/barefoot
RUN mkdir -p $OUTPUT_BASE/bin && \
    cp bazel-bin/$STRATUM_BF_BASE/stratum_bf $OUTPUT_BASE/bin/ && \
    cp $STRATUM_BF_BASE/platforms/*.json $OUTPUT_BASE/share/stratum/ && \
    cp $STRATUM_BF_BASE/docker/stratum-entrypoint.sh /output/ && \
    cp $STRATUM_BF_BASE/docker/stratum.flags $OUTPUT_BASE/share/stratum/ && \
    cp $STRATUM_BF_BASE/*.conf $OUTPUT_BASE/share/stratum/ && \
    touch $OUTPUT_BASE/share/stratum/phal_config.pb.txt && \
    cp $STRATUM_BF_BASE/chassis_config.pb.txt $OUTPUT_BASE/share/stratum/ && \
    mkdir -p /output/stratum/hal/lib/common/ && \
    cp stratum/hal/lib/common/gnmi_caps.pb.txt /output/stratum/hal/lib/common/
    # TODO(Yi Tseng): Remove two lines above when we are able to generate cap response automatically

FROM bitnami/minideb:stretch
LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This Docker image includes runtime library for stratum_bf"

COPY --from=builder /output /

RUN install_packages \
    libssl1.1 \
    libedit2 \
    libjudydebian1 \
    libboost-thread1.62.0 && \
    ldconfig && \
    mkdir /stratum_configs && \
    mkdir /stratum_logs

ENV BF_SDE_INSTALL /usr/local
EXPOSE 28000/tcp
ENTRYPOINT /stratum-entrypoint.sh
