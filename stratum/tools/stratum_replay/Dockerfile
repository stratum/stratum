# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FROM bitnami/minideb:buster
ADD ./stratum_replay /

LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="Docker-based distribution of the stratum_replay tool"

ENTRYPOINT [ "/stratum_replay" ]
