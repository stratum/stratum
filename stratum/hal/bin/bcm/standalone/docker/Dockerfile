# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FROM bitnami/minideb:buster
LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This is the Stratum runtime Docker image for Broadcom switches"

ARG STRATUM_TARGET=stratum_bcm
ADD ./${STRATUM_TARGET}_deb.deb /
RUN install_packages ./${STRATUM_TARGET}_deb.deb
RUN rm ./${STRATUM_TARGET}_deb.deb

EXPOSE 9339/tcp
EXPOSE 9559/tcp
ENTRYPOINT ["start-stratum.sh"]
