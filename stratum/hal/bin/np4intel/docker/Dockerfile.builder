#
# Copyright 2019-present Dell EMC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

ARG BAZEL_VERSION=2.2.0
ARG JDK_URL=https://mirror.bazel.build/openjdk/azul-zulu11.29.3-ca-jdk11.0.2/zulu11.29.3-ca-jdk11.0.2-linux_x64.tar.gz
ARG LLVM_REPO_NAME="deb http://apt.llvm.org/stretch/  llvm-toolchain-stretch main"
ARG PROTOBUF_TAG=v3.7.1
ARG GRPC_TAG=v1.21.3
ARG DPDK_VERSION=v19.08-rc4
ARG JOBS=4
ARG NP4_BIN
ARG NP4_DIR=/np4_intel

FROM ubuntu:18.04
LABEL maintainer="Stratum Ubuntu dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This Docker image sets up a development environment for Stratum on Ubuntu"

# Copy in the NP4 binary
ARG NP4_BIN
COPY $NP4_BIN /

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
    libjson-c3 libjson-c-dev cmake libhwloc-dev uuid-dev

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

# Install the NP4 Intel packages
ARG NP4_BIN
ARG NP4_DIR
RUN mkdir $NP4_DIR && mv /$NP4_BIN $NP4_DIR && \
    bash $NP4_DIR/$NP4_BIN offline

# Tools for style checking
RUN pip install setuptools wheel && \
    pip install cpplint && \
    pip install virtualenv

