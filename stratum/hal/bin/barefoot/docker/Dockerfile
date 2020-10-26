# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FROM bitnami/minideb:stretch
LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This Docker image includes runtime library for Barefoot Tofino switches"

ARG STRATUM_TARGET=stratum_bf
ADD ./${STRATUM_TARGET}_deb.deb /
RUN install_packages ./${STRATUM_TARGET}_deb.deb
RUN rm ./${STRATUM_TARGET}_deb.deb

RUN install_packages \
    kmod \
    libssl1.1 \
    libedit2 \
    libjudydebian1 \
    libboost-thread1.62.0 && \
    ldconfig && \
    mkdir -p /var/run/stratum

EXPOSE 28000/tcp
EXPOSE 9339/tcp
EXPOSE 9559/tcp

WORKDIR /var/run/stratum
ENTRYPOINT ["start-stratum.sh"]
