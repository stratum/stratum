# Copyright 2022-present Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

FROM bitnami/minideb:buster
LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This is the Stratum runtime Docker image for the P4 DPDK software switch"

ARG STRATUM_TARGET=stratum_dpdk
ADD ./${STRATUM_TARGET}_deb.deb /
RUN install_packages ./${STRATUM_TARGET}_deb.deb
RUN rm ./${STRATUM_TARGET}_deb.deb

EXPOSE 9339/tcp
EXPOSE 9559/tcp

WORKDIR /var/run/stratum
ENTRYPOINT ["start-stratum.sh"]
