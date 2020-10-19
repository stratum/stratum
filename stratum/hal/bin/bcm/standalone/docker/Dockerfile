# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FROM bitnami/minideb:stretch
LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This is the Stratum runtime Docker image for Broadcom switches"

ADD ./stratum_bcm_deb.deb /
RUN install_packages ./stratum_bcm_deb.deb
RUN rm ./stratum_bcm_deb.deb

EXPOSE 28000/tcp
EXPOSE 9339/tcp
EXPOSE 9559/tcp
ENTRYPOINT ["start-stratum.sh"]
