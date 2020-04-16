#
# Copyright 2020-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

FROM bitnami/minideb:stretch as builder

RUN mkdir -p /output
ADD ./stratum_bcm_deb.deb /
RUN dpkg -i --instdir=/output stratum_bcm_deb.deb

FROM bitnami/minideb:stretch
LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This is the Stratum runtime Docker image for Broadcom switches"

RUN install_packages \
    kmod \
    vim \
    wget \
    iproute2 \
    libssl1.1 \
    libedit2 \
    libjudydebian1 \
    libyaml-0-2 \
    libboost-thread1.62.0 && \
    ldconfig && \
    mkdir /stratum_logs

COPY --from=builder /output /

EXPOSE 28000/tcp
ENTRYPOINT start-stratum.sh
