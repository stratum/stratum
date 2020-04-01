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

ARG BAZEL_VERSION=2.2.0
ARG JDK_URL=https://mirror.bazel.build/openjdk/azul-zulu11.29.3-ca-jdk11.0.2/zulu11.29.3-ca-jdk11.0.2-linux_x64.tar.gz
ARG LLVM_REPO_NAME="deb http://apt.llvm.org/stretch/  llvm-toolchain-stretch main"
ARG PROTOBUF_TAG=v3.7.1
ARG GRPC_TAG=v1.21.3
ARG DPDK_VERSION=v19.08-rc4

# Reasonable for CI
ARG JOBS=2

FROM ubuntu:18.04
LABEL maintainer="Stratum Ubuntu dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This Docker image sets up a development environment for Stratum on Ubuntu"

ARG JOBS

# bazel dependencies
# + wget to download bazel binary
# + ca-certificates for wget HTPPS connection
# LLVM dependencies
# + gnupg2 for apt-key
# + software-properties-common for add-apt-repository
ENV PKG_DEPS pkg-config zip zlib1g-dev unzip python wget ca-certificates \
    ssh git gdb vim emacs-nox sudo libudev-dev libjudy-dev bison flex \
    libfl-dev libgmp-dev libi2c-dev python-yaml libyaml-dev build-essential \
    lcov curl autoconf automake libtool libgmp-dev libpcap-dev \
    libboost-thread-dev libboost-filesystem-dev libboost-program-options-dev \
    gnupg2 software-properties-common python-pip python-dev python3-dev \
    libfdt1 libnuma-dev libhugetlbfs-dev linux-virtual dkms \
    libjson-c-dev

RUN apt-get update && \
    apt-get install -y --no-install-recommends $PKG_DEPS

# LLVM toolchain
ARG LLVM_REPO_NAME
RUN wget --quiet -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN add-apt-repository "$LLVM_REPO_NAME"
RUN apt-get update && \
    apt-get install -y --no-install-recommends clang-format clang

ARG BAZEL_VERSION
RUN wget https://github.com/bazelbuild/bazel/releases/download/$BAZEL_VERSION/bazel-$BAZEL_VERSION-installer-linux-x86_64.sh && \
    chmod +x bazel-$BAZEL_VERSION-installer-linux-x86_64.sh && \
    ./bazel-$BAZEL_VERSION-installer-linux-x86_64.sh && \
    rm -f bazel-$BAZEL_VERSION-installer-linux-x86_64.sh

# Install java and lcov for Bazel coverage
ARG JDK_URL
ENV JAVA_HOME /usr/local/lib/jvm
RUN wget $JDK_URL -O $HOME/jdk_11.0.2.tar.gz && \
    mkdir -p $JAVA_HOME && \
    tar xf $HOME/jdk_11.0.2.tar.gz -C $JAVA_HOME --strip-components=1 && \
    rm $HOME/jdk_11.0.2.tar.gz
ENV PATH=$PATH:/usr/local/lib/jvm/bin

# grpc, protobuf
ENV PKG_DEPS build-essential autoconf libtool pkg-config \
    libgflags-dev libgtest-dev \
    clang-5.0 libc++-dev

RUN apt-get update && \
    apt-get install -y --no-install-recommends $PKG_DEPS

# install protobuf
ARG PROTOBUF_TAG
RUN git clone https://github.com/protocolbuffers/protobuf.git /tmp/protobuf && \
    cd /tmp/protobuf  && \
    git checkout tags/${PROTOBUF_TAG} && \
    git submodule update --init --recursive && \
    ./autogen.sh && \
    ./configure && \
    make && make check && make install && ldconfig && \
    rm -rf /tmp/protobuf

# install grpc
ARG GRPC_TAG
RUN git clone https://github.com/grpc/grpc /tmp/grpc && \
    cd /tmp/grpc  && \
    git checkout tags/${GRPC_TAG} && \
    git submodule update --init && \
    make && make install && ldconfig && \
    rm -rf /tmp/grpc

# Create link to /lib/modules for this kernel
RUN cd /lib/modules && \
    ln -s *-generic `uname -r`

ARG DPDK_VERSION
ENV RTE_SDK /usr/local/share/dpdk
RUN git clone https://github.com/DPDK/dpdk.git /tmp/dpdk && \
    cd /tmp/dpdk && \
    git checkout $DPDK_VERSION && \
    make config T=x86_64-native-linuxapp-gcc && make && \
    make install && \
    rm -rf /tmp/dpdk

# Copy in the deploy key
ARG SSH_DEPLOY_KEY
RUN mkdir /root/.ssh/ && \
    chmod 700 /root/.ssh && \
    echo "${SSH_DEPLOY_KEY}" > /root/.ssh/id_rsa && \
    chmod 400 /root/.ssh/id_rsa

# make sure your domain is accepted
RUN touch /root/.ssh/known_hosts && \
    ssh-keyscan gitlab.com >> /root/.ssh/known_hosts && \
    chmod 400 /root/.ssh/known_hosts

# Install the NP4 Intel packages
RUN git clone git@gitlab.com:open-pse/np4_intel_4_7_8.git /tmp/np4 && \
    cd /tmp/np4/ubuntu && \
    bash np4-intel-n3000-4.7.1-1-ubuntu.bin offline && \
    rm -rf /tmp/np4

# Remove the ssh deploy key
RUN rm /root/.ssh/id_rsa

# Tools for style checking
RUN pip install setuptools wheel && \
    pip install cpplint
