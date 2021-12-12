# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FROM debian:9@sha256:d8ee86bf0afeb901de293c692a540307a670306cbdb7a06e2c840f17b0c35374

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8

# Setup build environment.
ENV APT_DEPS \
  automake \
  clang \
  cmake \
  curl \
  git \
  libnuma-dev \
  libpcap-dev \
  locales \
  pkgconf \
  python3 \
  python3-pip \
  python3-pyelftools \
  python3-setuptools \
  sudo \
  wget \
  xz-utils

RUN apt update; apt upgrade -y
RUN apt-get install -y --no-install-recommends ${APT_DEPS}

ENV PIP_DEPS \
  meson \
  ninja

RUN pip3 install ${PIP_DEPS}

# Fetch DPDK source.
RUN curl -fsSL https://fast.dpdk.org/rel/dpdk-21.11.tar.xz | tar xJ
WORKDIR /dpdk-21.11

# Build DPDK install tarball.
RUN meson --prefix /dpdk-install build
WORKDIR /dpdk-21.11/build
RUN ninja
RUN ninja install
RUN ldconfig
RUN tar vcf /dpdk-install.tar.xz /dpdk-install/
