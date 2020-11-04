# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FROM bitnami/minideb:stretch
ADD ./stratum-replay /

LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="Docker-based distribution of the stratum-replay tool"

ENTRYPOINT [ "/stratum-replay" ]
