# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FROM bitnami/minideb:buster
LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This Docker image includes runtime library for Barefoot Tofino switches"

ARG STRATUM_TARGET=stratum_bfrt
ADD ./${STRATUM_TARGET}_deb.deb /
RUN install_packages ./${STRATUM_TARGET}_deb.deb
RUN rm ./${STRATUM_TARGET}_deb.deb

EXPOSE 9339/tcp
EXPOSE 9559/tcp

WORKDIR /var/run/stratum
ENTRYPOINT ["start-stratum.sh"]
