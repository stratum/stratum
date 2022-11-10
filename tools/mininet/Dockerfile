# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This Dockerfile expects the stratum root as its scope, hence you should build
# from root e.g.:
# docker build -t <some tag> -f tools/mininet/Dockerfile .

# We use a 2-stage build. Build all tools first, then copy only the strict necessary
# to a new image with runtime dependencies.
FROM stratumproject/build:build as builder

ENV BUILD_DEPS \
    python3-setuptools \
    python3-pip \
    help2man
RUN apt-get update && \
    apt-get install -y --no-install-recommends ${BUILD_DEPS}

# Build Mininet
RUN mkdir /tmp/mininet
WORKDIR /tmp/mininet
RUN curl -L https://github.com/mininet/mininet/tarball/master | \
    tar xz --strip-components 1
# Install in a special directory that we will copy to the runtime image.
RUN mkdir -p /output/usr/local/lib
RUN PREFIX=/output make install-mnexec install-manpages
RUN python3 setup.py install --root /output --prefix /usr/local
# Install `m` utility so user can attach to a mininet host directly
RUN cp util/m /output/bin/m && sed -i 's#sudo##g' /output/bin/m
# Install `host-cmd` utility so user can run commands on a host directly
COPY ./tools/mininet/host-cmd.sh /output/bin/host-cmd
RUN chmod +x /output/bin/host-cmd

# As well as the P4Runtime Python bindings installed by PI. This is not needed
# to run mininet, but it's useful to execute Python scripts acting as a
# P4Runtime client, e.g. control plane apps or PTF tests.
RUN pip3 install --root /output p4runtime==1.3.0

# Final stage, runtime.
FROM bitnami/minideb:buster as runtime

LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="Docker-based Mininet image that uses stratum_bmv2 as the default switch"

# Mininet and BMv2 simple_switch runtime dependencies
ENV RUNTIME_DEPS \
    iproute2 \
    iputils-ping \
    net-tools \
    ethtool \
    socat \
    psmisc \
    procps \
    iperf \
    arping \
    telnet \
    python3 \
    python3-pexpect \
    tcpdump \
    screen
RUN install_packages $RUNTIME_DEPS

COPY --from=builder /output /

ADD ./stratum_bmv2_deb.deb /
RUN install_packages /stratum_bmv2_deb.deb
ADD ./stratum_dpdk_deb.deb /
# Some files are present in both packages. Force overwrite them here.
RUN install_packages -o Dpkg::Options::="--force-overwrite" -f /stratum_dpdk_deb.deb
RUN ldconfig

WORKDIR /root
COPY stratum/hal/bin/bmv2/dummy.json ./dummy.json
COPY tools/mininet/stratum.py ./stratum.py
ENV PYTHONPATH /root:/usr/local/lib/python3.7/dist-packages:/usr/local/lib/python3.7/site-packages

# We need to expose one port per stratum_bmv2 instance, hence the number of
# exposed ports limit the number of switches that can be controlled from an
# external P4Runtime controller.
EXPOSE 50001-50100

ENTRYPOINT ["mn", "--custom", "/root/stratum.py", "--switch", "stratum-bmv2", "--host", "no-offload-host", "--controller", "none"]
