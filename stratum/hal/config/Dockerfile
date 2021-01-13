# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FROM bitnami/minideb:stretch
ADD ./chassis_config_migrator /

LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="Docker-based distribution of the Stratum chassis config migration tool"

ENTRYPOINT [ "/chassis_config_migrator" ]
