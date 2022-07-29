#
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#

# This Dockerfile expects its containing directory as its scope with the p4c-fpm
# Debian package present, hence you should build it like this:
# bazel build //stratum/p4c_backends/fpm:p4c_fpm_deb
# cp bazel-bin/stratum/p4c_backends/fpm/p4c_fpm_deb.deb stratum/p4c_backends/fpm
# cd stratum/p4c_backends/fpm
# docker build -t <some tag> -f Dockerfile  .

FROM bitnami/minideb:buster as builder

RUN mkdir -p /output
ADD ./p4c_fpm_deb.deb /
RUN dpkg -i --instdir=/output p4c_fpm_deb.deb

FROM bitnami/minideb:buster as runtime

LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="Docker-based distribution of the Stratum p4c FPM backend"

ENV RUNTIME_DEPS libgmp10 libgmpxx4ldbl gcc
RUN install_packages ${RUNTIME_DEPS}

COPY --from=builder /output /

ENTRYPOINT ["p4c-fpm"]
CMD ["-help"]
