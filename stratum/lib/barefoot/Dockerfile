# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FROM bitnami/minideb:stretch
# LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
# LABEL description="This Docker image includes runtime library for Barefoot Tofino switches"

RUN apt update && apt install build-essential -y

ADD ./stratum_bfrt_lib_deb.deb /
RUN install_packages ./stratum_bfrt_lib_deb.deb
RUN rm ./stratum_bfrt_lib_deb.deb

ADD stratum/lib/barefoot/examples/* /examples/
WORKDIR /examples
RUN make clean
RUN make test_c

# EXPOSE 28000/tcp
# EXPOSE 9339/tcp
# EXPOSE 9559/tcp

#WORKDIR /var/run/stratum
# ENTRYPOINT ["start-stratum.sh"]
