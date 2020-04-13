#
# Copyright 2019-present Open Networking Foundation
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

# This Dockerfile expects its containing directory as its scope with the p4c-fpm
# Debian package present, hence you should build it like this:
# bazel build //stratum/p4c_backends/fpm:p4c_fpm_deb
# cp bazel-bin/stratum/p4c_backends/fpm/p4c_fpm_deb.deb stratum/p4c_backends/fpm
# cd stratum/p4c_backends/fpm
# docker build -t <some tag> -f Dockerfile  .

FROM bitnami/minideb:stretch

LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="Docker-based distribution of the Stratum p4c FPM backend"

ENV RUNTIME_DEPS libgmp10 libgmpxx4ldbl gcc
RUN install_packages ${RUNTIME_DEPS}

ADD ./p4c_fpm_deb.deb /
RUN dpkg -i p4c_fpm_deb.deb

ENTRYPOINT ["p4c-fpm"]
CMD ["-help"]
